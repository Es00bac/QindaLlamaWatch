// SPDX-License-Identifier: GPL-3.0-or-later
//
// The sampler frame contract (src/host/sampler.sh <-> HostSampleParser) and
// the rates HostStats derives from consecutive frames.

#include "host/hostsampleparser.h"
#include "host/hoststats.h"

#include <QFile>
#include <QtTest>

using namespace QindaLlamaWatch;

namespace {

const char *const kHello =
    "@@hello\n=version 1\n=host qinda\n=netif tailscale0\n=clk_tck 100\n=gpu_name \n";

// Two CPUs; memory in kB; one llama-server with a DRM client.
QString frame(qint64 idle, qint64 user, double uptime, qint64 rx, qint64 llamaTicks)
{
    return QStringLiteral(
               "@@begin\n@@stat\n"
               "cpu  %1 0 0 %2 0 0 0 0 0 0\n"
               "cpu0 %3 0 0 %4 0 0 0 0 0 0\n"
               "cpu1 %3 0 0 %4 0 0 0 0 0 0\n"
               "@@meminfo\nMemTotal:       65536 kB\nMemFree:         1024 kB\n"
               "MemAvailable:   40960 kB\nCached:         30720 kB\nSwapTotal: 0 kB\nSwapFree: 0 kB\n"
               "@@scalars\n=loadavg 1.50 1.20 1.00 3/900 1234\n=uptime %5 99999.00\n=cpu_temp 71500\n"
               "@@freq\n4000000\n2000000\n"
               "@@scalars\n=net_rx %6\n=net_tx %6\n=gpu_busy 88\n=vram_used 1048576\n"
               "=vram_total 4194304\n=gtt_used 0\n=gpu_temp 74000\n=gpu_hot \n=gpu_power 318000000\n"
               "@@sclk\n0: 500Mhz \n1: 2480Mhz *\n@@mclk\n0: 96Mhz \n3: 1249Mhz *\n"
               "@@proc 812\n=rss_kib 2048\n=pstat 812 (llama-server) S 1 812 812 0 -1 4194560 1 0 0 0 %7 0 0 0 20 0 9 0\n"
               "=drm 7 1024 16\n=drm 7 1024 16\n"
               "@@end\n")
        .arg(user * 2).arg(idle * 2).arg(user).arg(idle)
        .arg(uptime, 0, 'f', 2).arg(rx).arg(llamaTicks);
}

QList<HostFrame> feedAll(HostSampleParser &parser, const QString &text)
{
    QList<HostFrame> frames;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        if (std::optional<HostFrame> f = parser.feed(line)) {
            frames.append(*f);
        }
    }
    return frames;
}

} // namespace

class TestHostSampleParser : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesIdentityAndFrame()
    {
        HostSampleParser parser;
        const QList<HostFrame> frames =
            feedAll(parser, QString::fromLatin1(kHello) + frame(100, 100, 10.0, 1000, 50));
        QCOMPARE(frames.size(), qsizetype(1));
        QCOMPARE(parser.identity().value(QStringLiteral("host")), QStringLiteral("qinda"));
        QCOMPARE(parser.identity().value(QStringLiteral("clk_tck")), QStringLiteral("100"));

        const HostFrame &f = frames.first();
        QCOMPARE(f.cpuRows.size(), qsizetype(3));
        QCOMPARE(f.meminfo.value(QStringLiteral("MemTotal")), 65536LL * 1024);
        QCOMPARE(f.freqKHz, (QList<qint64>{4000000, 2000000}));
        QCOMPARE(f.sclk, QStringLiteral("2480Mhz"));
        QCOMPARE(f.mclk, QStringLiteral("1249Mhz"));
        // "=gpu_hot " (a missing sysfs file) is absent, not zero.
        QVERIFY(!f.scalars.contains(QStringLiteral("gpu_hot")));
        QCOMPARE(f.procs.size(), qsizetype(1));
        QCOMPARE(f.procs.first().pid, 812LL);
        QCOMPARE(f.procs.first().rssKiB, 2048LL);
        QCOMPARE(f.procs.first().cpuTicks, 50LL);
        QCOMPARE(f.procs.first().drm.size(), qsizetype(2));
    }

    void ignoresLinesOutsideFrames()
    {
        HostSampleParser parser;
        QVERIFY(!parser.feed(QStringLiteral("=cpu_temp 1")).has_value());
        QVERIFY(!parser.feed(QStringLiteral("@@end")).has_value());
    }

    void procStatSurvivesParenthesesInName()
    {
        QCOMPARE(cpuTicksFromProcStat(
                     QStringLiteral("9 (a (b) c) S 1 9 9 0 -1 0 0 0 0 0 30 12 0 0 20 0 1 0")),
                 42LL);
        QCOMPARE(cpuTicksFromProcStat(QStringLiteral("garbage")), -1LL);
    }

    void derivesRatesFromConsecutiveFrames()
    {
        HostSampleParser parser;
        HostStats stats;
        const QList<HostFrame> frames = feedAll(
            parser, QString::fromLatin1(kHello) + frame(100, 100, 10.0, 1000, 50)
                        + frame(150, 150, 12.0, 5000, 250));
        QCOMPARE(frames.size(), qsizetype(2));

        const QVariantMap first = stats.update(frames.at(0), parser.identity());
        // No previous frame: no rates yet, and absent rather than 0.
        QVERIFY(!first.contains(QStringLiteral("cpu")));
        QVERIFY(!first.contains(QStringLiteral("netRx")));
        QVERIFY(!first.contains(QStringLiteral("llamaCpu")));

        const QVariantMap second = stats.update(frames.at(1), parser.identity());
        // +50 busy, +50 idle per CPU -> 50 %.
        QCOMPARE(second.value(QStringLiteral("cpu")).toDouble(), 50.0);
        QCOMPARE(second.value(QStringLiteral("cores")).toList().size(), qsizetype(2));
        // 4000 bytes over the host's own 2.00 s of uptime.
        QCOMPARE(second.value(QStringLiteral("netRx")).toDouble(), 2000.0);
        // 200 ticks at 100 Hz over 2 s = 1 core busy of 2 -> 50 % of the machine.
        QCOMPARE(second.value(QStringLiteral("llamaCpu")).toDouble(), 50.0);
        QCOMPARE(second.value(QStringLiteral("memUsed")).toDouble(), double((65536 - 40960) * 1024LL));
        QCOMPARE(second.value(QStringLiteral("cpuTemp")).toDouble(), 71.5);
        QCOMPARE(second.value(QStringLiteral("gpuWatts")).toDouble(), 318.0);
        QVERIFY(!second.contains(QStringLiteral("gpuHotspot")));
        QCOMPARE(second.value(QStringLiteral("load")).toString(), QStringLiteral("1.50 1.20 1.00"));
        // One DRM client counted once: 1024 KiB VRAM.
        QCOMPARE(second.value(QStringLiteral("llamaVram")).toDouble(), 1024.0 * 1024);
    }

    void serverRestartDropsNegativeTickDelta()
    {
        HostSampleParser parser;
        HostStats stats;
        const QList<HostFrame> frames = feedAll(
            parser, QString::fromLatin1(kHello) + frame(100, 100, 10.0, 1000, 5000)
                        + frame(150, 150, 12.0, 2000, 10));
        static_cast<void>(stats.update(frames.at(0), parser.identity()));
        const QVariantMap after = stats.update(frames.at(1), parser.identity());
        QVERIFY(!after.contains(QStringLiteral("llamaCpu")));
    }

    void nvidiaRowFillsGpuKeys()
    {
        HostFrame f;
        f.nvidiaCsv = QStringLiteral("45, 1024, 24576, 60, 250.5, 30, 1800, 9501, NVIDIA GeForce RTX 4090");
        HostStats stats;
        const QVariantMap out = stats.update(f, {});
        QCOMPARE(out.value(QStringLiteral("gpuBusy")).toDouble(), 45.0);
        QCOMPARE(out.value(QStringLiteral("vramTotal")).toDouble(), 24576.0 * 1024 * 1024);
        QCOMPARE(out.value(QStringLiteral("gpuName")).toString(), QStringLiteral("NVIDIA GeForce RTX 4090"));
    }

    void bundledScriptEmitsTheMarkersTheParserReads()
    {
        // AGENT-CONTRACT guard: the script and the parser agree on markers.
        QFile script(QStringLiteral(":/llamawatch/sampler.sh"));
        QVERIFY(script.open(QIODevice::ReadOnly));
        const QByteArray text = script.readAll();
        for (const char *marker : {"@@hello", "@@begin", "@@stat", "@@meminfo", "@@scalars",
                                   "@@freq", "@@sclk", "@@mclk", "@@nvidia", "@@proc", "@@end"}) {
            QVERIFY2(text.contains(marker), marker);
        }
    }
};

QTEST_GUILESS_MAIN(TestHostSampleParser)
#include "tst_hostsampleparser.moc"
