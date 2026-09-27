// SPDX-License-Identifier: GPL-3.0-or-later
//
// QindaLlamaWatch bootstrap. Composition (constructor-visible, GUI thread):
//
//   MonitorSettings         — where the server is (`monitorSettings` in QML)
//   LlamaClient             — HTTP poller of the llama.cpp API (`llama`)
//   HostProbe               — host CPU/GPU/RAM/VRAM over ssh (`hostProbe`)
//   ServerJournal           — request timings and log over ssh (`serverJournal`)
//   ApplicationCoordinator  — AppShell action/menu model (`coordinator`)
//   Main.qml (Tk.AppWindow) — QindaTK dashboard
//   composeFirstPartyMenuExport — the global menu; fail-closed, so the
//                               in-window Tk.MenuBar shows whenever the
//                               export is not live
//
// Destruction order is the reverse of declaration: the menu export dies
// before the window, the window (engine) before the collectors it binds to.

#include "app/action_catalog.h"
#include "host/hostprobe.h"
#include "journal/serverjournal.h"
#include "llama/llamaclient.h"
#include "settings/monitorsettings.h"

#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/app_shell/menu_export/first_party_composition.h>

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QIcon>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTimer>

#include <cstdio>
#include <memory>

using namespace QindaLlamaWatch;

namespace {

void configureCollectors(const MonitorSettings &settings, LlamaClient &llama, HostProbe &host,
                         ServerJournal &journal)
{
    llama.configure(QUrl::fromUserInput(settings.serverUrl()), settings.apiKey(), settings.intervalMs());
    host.configure(settings.sshHost(), settings.intervalMs());
    journal.configure(settings.sshHost(), settings.journalUnit(), settings.journalSudo());
}

} // namespace

int main(int argc, char **argv)
{
    // Tk.Graph traces are antialiased by multisampling; set before the first
    // window exists (as the QindaQt System Monitor does).
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("qindallamawatch"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("QindaLlamaWatch"));
    QGuiApplication::setOrganizationName(QStringLiteral("QindaQt"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.qindaqt.LlamaWatch"));
    QGuiApplication::setApplicationVersion(QStringLiteral(LLAMAWATCH_VERSION));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("org.qindaqt.LlamaWatch")));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("QindaLlamaWatch — live monitor for a remote llama.cpp server"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("server"),
                      QStringLiteral("llama.cpp server URL for this run (default: from Settings)"),
                      QStringLiteral("url")});
    parser.addOption({QStringLiteral("ssh"),
                      QStringLiteral("ssh destination of the server host for CPU/GPU/journal, "
                                     "\"local\" for this machine, \"\" to turn it off"),
                      QStringLiteral("host")});
    // Verification seam: render the window, save the settled frame, exit.
    parser.addOption({QStringLiteral("screenshot"),
                      QStringLiteral("Save a window screenshot to PATH after a few samples and exit"),
                      QStringLiteral("path")});
    parser.process(application);

    MonitorSettings settings;
    if (parser.isSet(QStringLiteral("server"))) {
        settings.overrideServer(parser.value(QStringLiteral("server")));
    }
    if (parser.isSet(QStringLiteral("ssh"))) {
        settings.overrideSshHost(parser.value(QStringLiteral("ssh")));
    }

    LlamaClient llama;
    HostProbe hostProbe;
    ServerJournal serverJournal;
    configureCollectors(settings, llama, hostProbe, serverJournal);
    QObject::connect(&settings, &MonitorSettings::changed, &llama,
                     [&settings, &llama, &hostProbe, &serverJournal] {
                         configureCollectors(settings, llama, hostProbe, serverJournal);
                     });

    QindaQt::AppShell::ApplicationCoordinator coordinator;
    coordinator.setApplicationName(QStringLiteral("QindaLlamaWatch"));
    if (const QindaQt::AppShell::Error error = coordinator.replaceActions(App::buildMonitorActions());
        !error.ok()) {
        qFatal("qindallamawatch: action catalog rejected: %s", qPrintable(error.message));
    }
    QObject::connect(&llama, &LlamaClient::sampled, &coordinator, [&coordinator, &llama] {
        const bool router = llama.routerMode();
        static_cast<void>(coordinator.setActionEnabled(QStringLiteral("server.load-next"), router));
        static_cast<void>(coordinator.setActionEnabled(QStringLiteral("server.unload"),
                                                       router && !llama.activeModel().isEmpty()));
        coordinator.setWindowTitle(llama.activeModel());
    });

    QQmlApplicationEngine engine;
    QQmlContext *context = engine.rootContext();
    context->setContextProperty(QStringLiteral("monitorSettings"), &settings);
    context->setContextProperty(QStringLiteral("llama"), &llama);
    context->setContextProperty(QStringLiteral("hostProbe"), &hostProbe);
    context->setContextProperty(QStringLiteral("serverJournal"), &serverJournal);
    context->setContextProperty(QStringLiteral("coordinator"), &coordinator);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &application,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("QindaLlamaWatch"), QStringLiteral("Main"));

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (window == nullptr) {
        return 1;
    }

    if (parser.isSet(QStringLiteral("screenshot"))) {
        const QString path = parser.value(QStringLiteral("screenshot"));
        const int delayMs = qEnvironmentVariableIntValue("LLAMAWATCH_SCREENSHOT_MS") > 0
            ? qEnvironmentVariableIntValue("LLAMAWATCH_SCREENSHOT_MS") : 5000;
        QTimer::singleShot(delayMs, window, [window, path] {
            if (window->grabWindow().save(path)) {
                std::fprintf(stdout, "screenshot=%s\n", qPrintable(path));
            } else {
                std::fprintf(stderr, "qindallamawatch: could not save screenshot to %s\n", qPrintable(path));
            }
            QCoreApplication::quit();
        });
    }

    QPointer<QQuickWindow> windowGuard(window);
    std::unique_ptr<QObject> menuExport = QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport(
        coordinator, *window, QDBusConnection::sessionBus(), [windowGuard](bool localMenuVisible) {
            if (windowGuard != nullptr) {
                windowGuard->setProperty("inWindowMenuVisible", localMenuVisible);
            }
        });

    const int result = QGuiApplication::exec();
    menuExport.reset();
    return result;
}
