#include "LanguagesMenu.h"

#include <QAction>
#include <QActionGroup>
#include <QChar>
#include <QHash>
#include <QMenu>
#include <QString>
#include <QWidget>

#include <algorithm>
#include <utility>
#include <vector>

LanguagesMenu::LanguagesMenu(QObject* parent)
    : QObject(parent) {}

QAction* LanguagesMenu::addLanguageAction(QMenu* parentMenu, const QString& label, const QString& lexerName) {
    QAction* action = parentMenu->addAction(label);
    action->setCheckable(true);
    action->setData(lexerName);
    m_group->addAction(action);
    connect(action, &QAction::triggered, this, &LanguagesMenu::onActionTriggered);
    return action;
}

QMenu* LanguagesMenu::createMenu(QWidget* menuParent) {
    m_menu = new QMenu(tr("&Languages"), menuParent);
    m_group = new QActionGroup(this);
    m_group->setExclusive(true);

    // --- Favorites (in this exact order) ---
    addLanguageAction(m_menu, tr("C#"),         QStringLiteral("cpp"));
    addLanguageAction(m_menu, tr("Python"),     QStringLiteral("python"));
    addLanguageAction(m_menu, tr("JavaScript"), QStringLiteral("javascript"));
    addLanguageAction(m_menu, tr("PHP"),        QStringLiteral("phpscript"));
    addLanguageAction(m_menu, tr("MySQL"),      QStringLiteral("sql"));
    addLanguageAction(m_menu, tr("JSON"),       QStringLiteral("json"));

    m_menu->addSeparator();

    // --- Plain Text first in the alphabetical section (directly under the menu) ---
    addLanguageAction(m_menu, tr("Plain Text"), QString());

    // --- Full alphabetical list of all languages ---
    std::vector<std::pair<QString, QString>> langs = {
        { tr("Bash"),             QStringLiteral("bash")       },
        { tr("Batch"),            QStringLiteral("batch")      },
        { tr("C / C++"),          QStringLiteral("cpp")        },
        { tr("CMake"),            QStringLiteral("cmake")      },
        { tr("CSS"),              QStringLiteral("css")        },
        { tr("Diff"),             QStringLiteral("diff")       },
        { tr("Dockerfile"),       QStringLiteral("bash")       },
        { tr("Erlang"),           QStringLiteral("erlang")     },
        { tr("Fortran"),          QStringLiteral("fortran")    },
        { tr("Go"),               QStringLiteral("cpp")        },
        { tr("Haskell"),          QStringLiteral("haskell")    },
        { tr("HTML"),             QStringLiteral("html")       },
        { tr("INI / Properties"), QStringLiteral("props")      },
        { tr("Java"),             QStringLiteral("cpp")        },
        { tr("Kotlin"),           QStringLiteral("cpp")        },
        { tr("LaTeX"),            QStringLiteral("latex")      },
        { tr("Lua"),              QStringLiteral("lua")        },
        { tr("Makefile"),         QStringLiteral("makefile")   },
        { tr("Markdown"),         QStringLiteral("markdown")   },
        { tr("Pascal"),           QStringLiteral("pascal")     },
        { tr("Perl"),             QStringLiteral("perl")       },
        { tr("PowerShell"),       QStringLiteral("powershell") },
        { tr("R"),                QStringLiteral("r")          },
        { tr("Ruby"),             QStringLiteral("ruby")       },
        { tr("Rust"),             QStringLiteral("rust")       },
        { tr("Scala"),            QStringLiteral("cpp")        },
        { tr("Shell"),            QStringLiteral("bash")       },
        { tr("SQL"),              QStringLiteral("sql")        },
        { tr("Swift"),            QStringLiteral("cpp")        },
        { tr("TCL"),              QStringLiteral("tcl")        },
        { tr("TOML"),             QStringLiteral("toml")       },
        { tr("TypeScript"),       QStringLiteral("javascript") },
        { tr("VB"),               QStringLiteral("vb")         },
        { tr("VHDL"),             QStringLiteral("vhdl")       },
        { tr("Verilog"),          QStringLiteral("verilog")    },
        { tr("XML"),              QStringLiteral("xml")        },
        { tr("YAML"),             QStringLiteral("yaml")       },
        { tr("Zig"),              QStringLiteral("zig")        },
    };

    // Sort the full list alphabetically (case-insensitive) by display name.
    std::sort(langs.begin(), langs.end(),
              [](const std::pair<QString, QString>& a,
                 const std::pair<QString, QString>& b) {
                  return a.first.compare(b.first, Qt::CaseInsensitive) < 0;
              });

    // Bucket by first letter (uppercased), preserving the sorted order within each bucket.
    QHash<QChar, std::vector<std::pair<QString, QString>>> buckets;
    std::vector<QChar> letterOrder;
    for (const auto& kv : langs) {
        if (kv.first.isEmpty()) {
            continue;
        }
        const QChar letter = kv.first.at(0).toUpper();
        if (!buckets.contains(letter)) {
            letterOrder.push_back(letter);
        }
        buckets[letter].push_back(kv);
    }

    // Resolve a per-letter translatable title. Each branch uses a literal
    // `tr("X")` so lupdate can extract every letter as its own translation key.
    auto letterTitle = [this](QChar c) -> QString {
        switch (c.unicode()) {
            case u'A': return tr("A");
            case u'B': return tr("B");
            case u'C': return tr("C");
            case u'D': return tr("D");
            case u'E': return tr("E");
            case u'F': return tr("F");
            case u'G': return tr("G");
            case u'H': return tr("H");
            case u'I': return tr("I");
            case u'J': return tr("J");
            case u'K': return tr("K");
            case u'L': return tr("L");
            case u'M': return tr("M");
            case u'N': return tr("N");
            case u'O': return tr("O");
            case u'P': return tr("P");
            case u'Q': return tr("Q");
            case u'R': return tr("R");
            case u'S': return tr("S");
            case u'T': return tr("T");
            case u'U': return tr("U");
            case u'V': return tr("V");
            case u'W': return tr("W");
            case u'X': return tr("X");
            case u'Y': return tr("Y");
            case u'Z': return tr("Z");
            default:   return QString(c);
        }
    };

    // Letters appear in alphabetical order because `langs` was already sorted
    // case-insensitively by display name, so the first occurrence of each
    // letter is encountered in alphabetical order.
    for (const QChar letter : letterOrder) {
        QMenu* sub = m_menu->addMenu(letterTitle(letter));
        for (const auto& kv : buckets[letter]) {
            addLanguageAction(sub, kv.first, kv.second);
        }
    }

    // Reflect the current "no editor" state until setActiveEditor is called.
    m_menu->setEnabled(m_editor != nullptr);

    return m_menu;
}

void LanguagesMenu::setActiveEditor(ScintillaEdit* editor) {
    m_editor = editor;
    if (m_menu) {
        m_menu->setEnabled(editor != nullptr);
    }
}

namespace {
// Recursively walk a menu's actions (descending into submenus) and return the
// first action whose data() matches `lexerName`.
QAction* findActionByLexer(QMenu* menu, const QString& lexerName) {
    if (!menu) {
        return nullptr;
    }
    const auto actions = menu->actions();
    for (QAction* action : actions) {
        if (action->isSeparator()) {
            continue;
        }
        if (QMenu* sub = action->menu()) {
            if (QAction* hit = findActionByLexer(sub, lexerName)) {
                return hit;
            }
            continue;
        }
        if (action->data().toString() == lexerName) {
            return action;
        }
    }
    return nullptr;
}
} // namespace

void LanguagesMenu::syncCheckedLanguage(const QString& lexerName) {
    if (!m_menu) {
        return;
    }

    // Walk all actions in menu order, recursing into submenus. The first match wins.
    QAction* matched = findActionByLexer(m_menu, lexerName);

    if (matched) {
        matched->setChecked(true);
    } else {
        // No match: uncheck all. With an exclusive QActionGroup we have to
        // temporarily disable exclusivity to clear the current selection.
        if (QAction* checked = m_group->checkedAction()) {
            const bool wasExclusive = m_group->isExclusive();
            m_group->setExclusive(false);
            checked->setChecked(false);
            m_group->setExclusive(wasExclusive);
        }
    }
}

void LanguagesMenu::onActionTriggered() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action) {
        return;
    }
    emit languageSelected(action->data().toString());
}
