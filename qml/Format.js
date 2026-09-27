// SPDX-License-Identifier: GPL-3.0-or-later
.pragma library

// Display formatting shared by every panel. A reading the collectors did not
// report is `undefined`; it renders as a dash, never as 0 (a zero is a real
// measurement and must stay distinguishable from "unknown").

var dash = "—"

function known(v) {
    return v !== undefined && v !== null && !(typeof v === "number" && isNaN(v))
}

function bytes(n) {
    if (!known(n)) return dash
    var units = ["B", "KiB", "MiB", "GiB", "TiB"]
    var i = 0
    while (Math.abs(n) >= 1024 && i < units.length - 1) {
        n /= 1024
        i++
    }
    return (i < 2 ? n.toFixed(0) : n.toFixed(1)) + " " + units[i]
}

function rate(n) {
    return known(n) ? bytes(n) + "/s" : dash
}

function tps(n, digits) {
    return known(n) ? n.toFixed(digits === undefined ? 1 : digits) + " t/s" : dash
}

function percent(n, digits) {
    return known(n) ? n.toFixed(digits === undefined ? 0 : digits) + "%" : dash
}

function number(n, digits) {
    return known(n) ? n.toFixed(digits === undefined ? 0 : digits) : dash
}

// 12345 -> "12,345"
function count(n) {
    if (!known(n)) return dash
    return Math.round(n).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")
}

// 1536 -> "1.5k", 131072 -> "128k"
function kilo(n) {
    if (!known(n)) return dash
    if (n < 1000) return String(Math.round(n))
    var k = n / 1024
    return (k >= 10 ? Math.round(k) : k.toFixed(1)) + "k"
}

// plural(3, "crash", "crashes") -> "3 crashes"
function plural(n, one, many) {
    return n + " " + (n === 1 ? one : many)
}

function celsius(n) {
    return known(n) ? n.toFixed(0) + "°C" : dash
}

function duration(seconds) {
    if (!known(seconds)) return dash
    seconds = Math.floor(seconds)
    var d = Math.floor(seconds / 86400)
    var h = Math.floor((seconds % 86400) / 3600)
    var m = Math.floor((seconds % 3600) / 60)
    var s = seconds % 60
    if (d > 0) return d + "d " + h + "h"
    if (h > 0) return h + "h " + (m < 10 ? "0" : "") + m + "m"
    return m + "m " + (s < 10 ? "0" : "") + s + "s"
}

function basename(path) {
    if (!known(path) || path === "") return dash
    var parts = String(path).split("/")
    return parts[parts.length - 1]
}
