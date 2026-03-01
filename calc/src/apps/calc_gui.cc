// WEBKIT_DISABLE_COMPOSITING_MODE=1 ./build/src/apps/calc-gui
#include "calculator.h"
#include <webview/webview.h>

#include <format>
#include <string>
#include <iostream>

constexpr const char *html = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>

body {
    background: #1e1e1e;
    color: white;
    font-family: sans-serif;
    display: flex;
    flex-direction: column;
    align-items: center;
    margin: 0;
}

#display {
    width: 90%;
    height: 60px;
    margin: 20px;
    font-size: 24px;
    text-align: right;
    padding: 10px;
    background: #2d2d2d;
    border: none;
    color: white;
}

.grid {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 10px;
    width: 90%;
}

button {
    padding: 20px;
    font-size: 20px;
    border: none;
    background: #3a3a3a;
    color: white;
    cursor: pointer;
}

button:hover {
    background: #555;
}

.equal {
    background: #ff9800;
}
</style>
</head>
<body>

<input id="display" readonly />
<div class="grid">
    <button onclick="press('(')">(</button>
    <button onclick="press(')')">)</button>
    <button onclick="backspace()">⌫</button>
    <button onclick="press('/')">÷</button>

    <button onclick="press('7')">7</button>
    <button onclick="press('8')">8</button>
    <button onclick="press('9')">9</button>
    <button onclick="press('*')">×</button>

    <button onclick="press('4')">4</button>
    <button onclick="press('5')">5</button>
    <button onclick="press('6')">6</button>
    <button onclick="press('-')">−</button>

    <button onclick="press('1')">1</button>
    <button onclick="press('2')">2</button>
    <button onclick="press('3')">3</button>
    <button onclick="press('+')">+</button>

    <button onclick="press('.')">.</button>
    <button onclick="press('0')">0</button>
    <button onclick="clearAll()">AC</button>
    <button onclick="calculate()" class="equal" style="background:#ff9800;">=</button>
</div>
<script>
let lastWasError = false;

function press(val) {
    if (lastWasError) clearAll();
    const display = document.getElementById("display");
    display.value += val;
}

function clearAll() {
    document.getElementById("display").value = "";
    lastWasError = false;
}

function backspace() {
    if (lastWasError) clearAll();
    else {
        const display = document.getElementById("display");
        display.value = display.value.slice(0, -1);
    }
}

async function calculate() {
    const display = document.getElementById("display");
    const expr = display.value;
    try {
        const data = await window.evaluate(expr);
        display.value = data.value;
        lastWasError = data.error;
    } catch (e) {
        display.value = "Unexpected error: " + e;
        lastWasError = true;
    }
}
</script>

</body>
</html>
)HTML";

int main() try {
    webview::webview w{false, nullptr};

    w.set_title("Calc GUI");
    w.set_size(480, 640, WEBVIEW_HINT_NONE);

    // Bind C++ function callable from JS
    Calc::Calculator calc;
    w.bind("evaluate", [&calc](const std::string& json) {
        auto expr = json.substr(2, json.size() - 4);
        auto res = calc.compute(expr);
        if (res.has_value())
            return std::format(R"({{"error":false,"value":{}}})", *res);
        else {
            std::string err = res.error();
            std::string escaped;
            for (char c : err) {
                if (c == '"') escaped += "\\\"";
                else escaped += c;
            }
            return std::format(R"({{"error":true,"value":"{}"}})", escaped);
        }
    });

    w.set_html(html);
    w.run();

    return 0;
}

catch (const webview::exception &ex) {
    std::cerr << ex.what() << "\n";
}
