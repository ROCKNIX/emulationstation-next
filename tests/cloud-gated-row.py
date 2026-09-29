"""A cloud row greyed for want of a setup does what it says once setup is done
(audit #307 PL-062).

The rows under MANAGE CLOUD STORAGE and its pages that need a cloud decide
"configured" when their page is built. Setup is opened from them and ends
in CLOUD SETUP COMPLETE, whose FINISH closes back to the page underneath --
the same page, built before rclone.conf existed, whose rows still offered
the setup. This compiles the shipped cloudAddGatedEntry out of GuiMenu.cpp
unchanged, with doubles for the page, the row and the dialog, builds a row
while nothing is set up, writes the config the way setup does, and presses
the row.

    python3 tests/cloud-gated-row.py [path/to/GuiMenu.cpp]
"""
from pathlib import Path
import subprocess
import sys
import tempfile

source_path = (Path(sys.argv[1]) if len(sys.argv) > 1 else
               Path(__file__).resolve().parents[1] / "es-app/src/guis/GuiMenu.cpp")
source = source_path.read_text()


def definition(signature):
    """The body of the first definition (not declaration) of `signature`."""
    start = source.index(signature)
    while source.index(';', start) < source.index('{', start):
        start = source.index(signature, start + 1)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        if source[end] == '{':
            depth += 1
        elif source[end] == '}':
            depth -= 1
        end += 1
    return source[start:end]


harness = r'''
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#define _(value) std::string(value)

static bool gConfigured = false;   // whether rclone.conf exists, as setup leaves it
namespace Utils {
namespace String { std::string toUpper(const std::string& v) { return v; } }
namespace FileSystem {
bool exists(const std::string& path, bool = true) { return path == "/storage/.config/rclone/rclone.conf" && gConfigured; }
}
}
struct Gui { virtual ~Gui() {} std::string what; };
struct Window {
    std::vector<std::unique_ptr<Gui>> stack;
    void pushGui(Gui* g) { stack.emplace_back(g); }
};
struct GuiMsgBox : Gui {
    GuiMsgBox(Window*, const std::string& text, const std::string&, const std::function<void()>&,
              const std::string&, const std::function<void()>&) { what = "dialog: " + text; }
};
struct GuiComponent { virtual ~GuiComponent() {} };
struct MultiLineMenuEntry : GuiComponent {
    bool dimmed = false;
    MultiLineMenuEntry(Window*, const std::string&, const std::string&, bool) {}
    void setDimmed(bool d) { dimmed = d; }
};
struct ComponentListRow {
    bool selectable = true;
    std::vector<std::shared_ptr<GuiComponent>> elements;
    std::function<void()> accept;
    void addElement(const std::shared_ptr<GuiComponent>& e, bool) { elements.push_back(e); }
    void makeAcceptInputHandler(const std::function<void()>& f) { accept = f; }
};
struct GuiSettings {
    std::vector<ComponentListRow> rows;
    void addRow(const ComponentListRow& r) { rows.push_back(r); }
    template <class... More>
    void addWithDescription(const std::string&, const std::string&, std::nullptr_t, const std::function<void()>& f, More...) {
        ComponentListRow r; r.accept = f; rows.push_back(r);
    }
};
namespace GuiMenu { static int opened = 0; void openCloudAddRemote(Window*) { opened++; } }
''' + definition('static void cloudAddGatedEntry(') + r'''
int main()
{
    Window window;
    GuiSettings page;
    int ran = 0;
    gConfigured = false;
    cloudAddGatedEntry(&page, &window, false, "BACK UP SAVES TO THE CLOUD", "LAST: NEVER", [&ran] { ran++; });
    auto entry = std::dynamic_pointer_cast<MultiLineMenuEntry>(page.rows.at(0).elements.at(0));

    int failures = 0;
    // Before setup: the press offers setup and runs nothing.
    page.rows.at(0).accept();
    if (ran != 0 || window.stack.size() != 1) { std::cout << "FAIL before setup: ran=" << ran << " dialogs=" << window.stack.size() << "\n"; failures++; }
    else std::cout << "ok   before setup: the press offers setup (" << window.stack.back()->what.substr(0, 48) << "...)\n";

    // Setup completes underneath: rclone.conf is written, FINISH closes
    // back to this page, which was built before it existed.
    window.stack.clear();
    gConfigured = true;
    page.rows.at(0).accept();
    if (ran != 1) { std::cout << "FAIL after setup: the press ran the row " << ran << " times and pushed " << window.stack.size() << " page(s)"
                              << (window.stack.empty() ? "" : " -- " + window.stack.back()->what.substr(0, 60)) << "\n"; failures++; }
    else std::cout << "ok   after setup: the press runs the row\n";
    if (entry && entry->dimmed) { std::cout << "FAIL after setup: the row still draws dimmed\n"; failures++; }
    else std::cout << "ok   after setup: the row no longer draws dimmed\n";
    std::cout << "cloud-gated-row: " << (failures ? "FAIL" : "PASS") << "\n";
    return failures ? 1 : 0;
}
'''

with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    cpp = tmp / "harness.cpp"
    exe = tmp / "harness"
    cpp.write_text(harness)
    subprocess.run(["g++", "-std=c++17", "-fsanitize=address,undefined", "-o", str(exe), str(cpp)], check=True)
    sys.exit(subprocess.run([str(exe)]).returncode)
