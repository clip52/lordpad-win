#include "TabExtras.h"

#include "EditorTab.h"
#include "ScintillaEdit.h"

#include <QAction>
#include <QColor>
#include <QColorDialog>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QSettings>
#include <QStringList>
#include <QTabBar>
#include <QTabWidget>
#include <QVariant>
#include <QWidget>

namespace {
constexpr const char* kSettingsOrg   = "clip52";
constexpr const char* kSettingsApp   = "notepadpp-qt";
constexpr const char* kSettingsGroup = "TabExtras";
constexpr const char* kKeyPinned     = "pinned";
constexpr const char* kKeyLocked     = "locked";
constexpr const char* kKeyColors     = "colors";  // a sub-group of "<hexpath>" -> "#RRGGBB"

// Helper: encode a path so it can be a QSettings sub-key (no "/" mess).
QString encodePathKey(const QString& abs) {
    return QString::fromLatin1(abs.toUtf8().toHex());
}
QString decodePathKey(const QString& enc) {
    return QString::fromUtf8(QByteArray::fromHex(enc.toLatin1()));
}
} // namespace

// ---------------------------------------------------------------------------

TabExtras::TabExtras(QObject* parent)
    : QObject(parent) {
    loadState();
}

TabExtras::~TabExtras() = default;

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

void TabExtras::loadState() {
    QSettings s(kSettingsOrg, kSettingsApp);
    s.beginGroup(kSettingsGroup);

    const QStringList pinned = s.value(kKeyPinned).toStringList();
    for (const QString& p : pinned) m_pinned.insert(p);

    const QStringList locked = s.value(kKeyLocked).toStringList();
    for (const QString& p : locked) m_locked.insert(p);

    s.beginGroup(kKeyColors);
    const QStringList colorKeys = s.childKeys();
    for (const QString& k : colorKeys) {
        const QString path = decodePathKey(k);
        const QString hex  = s.value(k).toString();
        if (!path.isEmpty() && !hex.isEmpty())
            m_colors.insert(path, hex);
    }
    s.endGroup();

    s.endGroup();
}

void TabExtras::savePinned() {
    QSettings s(kSettingsOrg, kSettingsApp);
    s.beginGroup(kSettingsGroup);
    s.setValue(kKeyPinned, QStringList(m_pinned.values()));
    s.endGroup();
}

void TabExtras::saveLocked() {
    QSettings s(kSettingsOrg, kSettingsApp);
    s.beginGroup(kSettingsGroup);
    s.setValue(kKeyLocked, QStringList(m_locked.values()));
    s.endGroup();
}

void TabExtras::saveColors() {
    QSettings s(kSettingsOrg, kSettingsApp);
    s.beginGroup(kSettingsGroup);
    s.remove(kKeyColors);
    s.beginGroup(kKeyColors);
    for (auto it = m_colors.constBegin(); it != m_colors.constEnd(); ++it)
        s.setValue(encodePathKey(it.key()), it.value());
    s.endGroup();
    s.endGroup();
}

// ---------------------------------------------------------------------------
// Wiring
// ---------------------------------------------------------------------------

void TabExtras::attachTabWidget(QTabWidget* tw) {
    if (!tw) return;
    for (const auto& g : m_groups)
        if (g.data() == tw) return;  // already attached

    m_groups.append(QPointer<QTabWidget>(tw));
    if (!m_lastActive) m_lastActive = tw;

    // Right-click anywhere on the tab bar pops our menu.
    if (QTabBar* bar = tw->tabBar()) {
        bar->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(bar, &QWidget::customContextMenuRequested,
                this, &TabExtras::onCustomContextMenu, Qt::UniqueConnection);
    }
    connect(tw, &QTabWidget::currentChanged,
            this, &TabExtras::onCurrentChanged, Qt::UniqueConnection);
    connect(tw, &QObject::destroyed,
            this, &TabExtras::onTabWidgetDestroyed, Qt::UniqueConnection);

    // Apply any persisted state to tabs already present.
    for (int i = 0; i < tw->count(); ++i)
        applyPersistedTo(tw, i);
}

void TabExtras::onTabWidgetDestroyed(QObject* obj) {
    for (int i = m_groups.size() - 1; i >= 0; --i)
        if (!m_groups[i] || m_groups[i].data() == obj)
            m_groups.removeAt(i);
}

void TabExtras::onCurrentChanged(int /*index*/) {
    if (auto* tw = qobject_cast<QTabWidget*>(sender()))
        m_lastActive = tw;
}

// ---------------------------------------------------------------------------
// Lookups
// ---------------------------------------------------------------------------

EditorTab* TabExtras::editorTabAt(QTabWidget* tw, int index) const {
    if (!tw || index < 0 || index >= tw->count()) return nullptr;
    return qobject_cast<EditorTab*>(tw->widget(index));
}

QString TabExtras::pathAt(QTabWidget* tw, int index) const {
    if (auto* et = editorTabAt(tw, index)) {
        const QString p = et->filePath();
        if (!p.isEmpty()) return p;
    }
    // Fallback: an "untitled" buffer can still be tracked by its tab text,
    // but persistence only kicks in for real paths.
    return {};
}

QTabWidget* TabExtras::resolveGroup(QTabWidget* tw) const {
    if (tw) return tw;
    if (m_lastActive) return m_lastActive.data();
    for (const auto& g : m_groups)
        if (g) return g.data();
    return nullptr;
}

// ---------------------------------------------------------------------------
// Visuals
// ---------------------------------------------------------------------------

QIcon TabExtras::pinIcon() const {
    QIcon ic = QIcon::fromTheme(QStringLiteral("pin"));
    if (!ic.isNull()) return ic;
    ic = QIcon::fromTheme(QStringLiteral("window-pin"));
    if (!ic.isNull()) return ic;

    // Fallback: hand-drawn pin glyph so we never ship a blank icon.
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(60, 60, 60), 1.2));
    p.setBrush(QColor(220, 70, 70));
    // pin head
    p.drawEllipse(QPointF(8, 6), 4.0, 4.0);
    // shaft
    p.drawLine(8, 10, 8, 14);
    return QIcon(pm);
}

void TabExtras::refreshTabDecoration(QTabWidget* tw, int index) {
    if (!tw || index < 0 || index >= tw->count()) return;
    const QString path = pathAt(tw, index);

    // Icon: pinned tabs get the pin glyph; otherwise we leave it as-is. We
    // don't try to remember "the previous icon" because the rest of the app
    // currently doesn't set per-tab icons.
    if (m_pinned.contains(path))
        tw->setTabIcon(index, pinIcon());
    else
        tw->setTabIcon(index, QIcon());

    // Color: per-tab text color (visible on QTabBar).
    if (QTabBar* bar = tw->tabBar()) {
        if (m_colors.contains(path))
            bar->setTabTextColor(index, QColor(m_colors.value(path)));
        else
            bar->setTabTextColor(index, QColor());  // invalid -> default
    }

    // Tooltip hint for locked tabs (cheap, useful, no extra widgets).
    if (m_locked.contains(path)) {
        const QString cur = tw->tabToolTip(index);
        const QString tag = tr("[Bloqueado] ");
        if (!cur.startsWith(tag))
            tw->setTabToolTip(index, tag + cur);
    }
}

void TabExtras::applyPersistedTo(QTabWidget* tw, int index) {
    auto* et = editorTabAt(tw, index);
    if (!et) return;
    const QString path = et->filePath();
    if (path.isEmpty()) return;

    if (m_locked.contains(path) && et->editor())
        et->editor()->send(SCI_SETREADONLY, 1, 0);

    refreshTabDecoration(tw, index);

    // If pinned and not yet at slot 0 (or among the pinned prefix), move it.
    if (m_pinned.contains(path) && index > 0) {
        if (QTabBar* bar = tw->tabBar()) bar->moveTab(index, 0);
    }
}

// ---------------------------------------------------------------------------
// Per-tab operations
// ---------------------------------------------------------------------------

void TabExtras::pinTab(QTabWidget* tw, int index) {
    tw = resolveGroup(tw);
    if (!tw) return;
    const QString path = pathAt(tw, index);
    if (path.isEmpty()) return;

    const bool wasPinned = m_pinned.contains(path);
    if (wasPinned) {
        m_pinned.remove(path);
    } else {
        m_pinned.insert(path);
        if (QTabBar* bar = tw->tabBar()) {
            // Slot 0 keeps the pin "row" left-anchored, classic Notepad++ feel.
            bar->moveTab(index, 0);
            index = 0;
        }
    }
    savePinned();
    refreshTabDecoration(tw, index);
}

void TabExtras::lockTab(QTabWidget* tw, int index) {
    tw = resolveGroup(tw);
    if (!tw) return;
    auto* et = editorTabAt(tw, index);
    if (!et || !et->editor()) return;
    const QString path = et->filePath();

    const bool nowLocked = !m_locked.contains(path);
    et->editor()->send(SCI_SETREADONLY, nowLocked ? 1 : 0, 0);

    if (!path.isEmpty()) {
        if (nowLocked) m_locked.insert(path);
        else           m_locked.remove(path);
        saveLocked();
    }
    refreshTabDecoration(tw, index);
}

void TabExtras::colorTab(QTabWidget* tw, int index) {
    tw = resolveGroup(tw);
    if (!tw) return;
    QTabBar* bar = tw->tabBar();
    if (!bar) return;

    const QString path = pathAt(tw, index);
    QColor initial = bar->tabTextColor(index);
    if (!initial.isValid()) initial = QColor("#1976d2");

    const QColor chosen = QColorDialog::getColor(
        initial, tw, tr("Cor da aba"),
        QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid()) return;

    bar->setTabTextColor(index, chosen);
    if (!path.isEmpty()) {
        m_colors.insert(path, chosen.name());
        saveColors();
    }
}

void TabExtras::closeOthers(QTabWidget* tw, int index) {
    tw = resolveGroup(tw);
    if (!tw || index < 0 || index >= tw->count()) return;

    QWidget* keep = tw->widget(index);
    // Walk from the end so signal-driven removals don't invalidate our walk.
    for (int i = tw->count() - 1; i >= 0; --i) {
        if (tw->widget(i) == keep) continue;
        const QString p = pathAt(tw, i);
        if (m_pinned.contains(p)) continue;  // pinned tabs survive bulk close
        emit tw->tabCloseRequested(i);
    }
}

void TabExtras::closeToRight(QTabWidget* tw, int index) {
    tw = resolveGroup(tw);
    if (!tw || index < 0) return;
    for (int i = tw->count() - 1; i > index; --i) {
        const QString p = pathAt(tw, i);
        if (m_pinned.contains(p)) continue;
        emit tw->tabCloseRequested(i);
    }
}

void TabExtras::closeToLeft(QTabWidget* tw, int index) {
    tw = resolveGroup(tw);
    if (!tw || index <= 0) return;
    for (int i = index - 1; i >= 0; --i) {
        const QString p = pathAt(tw, i);
        if (m_pinned.contains(p)) continue;
        emit tw->tabCloseRequested(i);
    }
}

bool TabExtras::isPinned(const QString& absPath) const {
    return m_pinned.contains(absPath);
}

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------

void TabExtras::onCustomContextMenu(const QPoint& pos) {
    auto* bar = qobject_cast<QTabBar*>(sender());
    if (!bar) return;

    // Find the owning QTabWidget — bar's parent.
    auto* tw = qobject_cast<QTabWidget*>(bar->parentWidget());
    if (!tw) return;

    const int idx = bar->tabAt(pos);
    if (idx < 0) return;

    m_lastActive = tw;
    const QString path = pathAt(tw, idx);
    const bool pinned = m_pinned.contains(path);
    const bool locked = m_locked.contains(path);

    QMenu menu(tw);

    QAction* aPin = menu.addAction(pinned ? tr("Desafixar aba") : tr("Fixar aba"));
    aPin->setCheckable(true);
    aPin->setChecked(pinned);

    QAction* aLock = menu.addAction(locked ? tr("Desbloquear (somente leitura)")
                                           : tr("Bloquear (somente leitura)"));
    aLock->setCheckable(true);
    aLock->setChecked(locked);

    QAction* aColor = menu.addAction(tr("Cor da aba..."));

    menu.addSeparator();
    QAction* aOthers = menu.addAction(tr("Fechar outras"));
    QAction* aRight  = menu.addAction(tr("Fechar à direita"));
    QAction* aLeft   = menu.addAction(tr("Fechar à esquerda"));
    menu.addSeparator();
    QAction* aClose  = menu.addAction(tr("Fechar"));

    QAction* picked = menu.exec(bar->mapToGlobal(pos));
    if (!picked) return;

    if      (picked == aPin)    pinTab(tw, idx);
    else if (picked == aLock)   lockTab(tw, idx);
    else if (picked == aColor)  colorTab(tw, idx);
    else if (picked == aOthers) closeOthers(tw, idx);
    else if (picked == aRight)  closeToRight(tw, idx);
    else if (picked == aLeft)   closeToLeft(tw, idx);
    else if (picked == aClose)  emit tw->tabCloseRequested(idx);
}

// ---------------------------------------------------------------------------
// Action factories — each one captures `this` and operates on the current
// tab of the most-recently-active group at trigger time.
// ---------------------------------------------------------------------------

QAction* TabExtras::makePinAction(QObject* parent) {
    auto* a = new QAction(tr("Fixar aba"), parent);
    a->setIcon(pinIcon());
    connect(a, &QAction::triggered, this, [this]() {
        QTabWidget* tw = resolveGroup(nullptr);
        if (tw) pinTab(tw, tw->currentIndex());
    });
    return a;
}

QAction* TabExtras::makeLockAction(QObject* parent) {
    auto* a = new QAction(tr("Bloquear aba (somente leitura)"), parent);
    connect(a, &QAction::triggered, this, [this]() {
        QTabWidget* tw = resolveGroup(nullptr);
        if (tw) lockTab(tw, tw->currentIndex());
    });
    return a;
}

QAction* TabExtras::makeColorAction(QObject* parent) {
    auto* a = new QAction(tr("Cor da aba..."), parent);
    connect(a, &QAction::triggered, this, [this]() {
        QTabWidget* tw = resolveGroup(nullptr);
        if (tw) colorTab(tw, tw->currentIndex());
    });
    return a;
}

QAction* TabExtras::makeCloseOthersAction(QObject* parent) {
    auto* a = new QAction(tr("Fechar outras abas"), parent);
    connect(a, &QAction::triggered, this, [this]() {
        QTabWidget* tw = resolveGroup(nullptr);
        if (tw) closeOthers(tw, tw->currentIndex());
    });
    return a;
}

QAction* TabExtras::makeCloseToRightAction(QObject* parent) {
    auto* a = new QAction(tr("Fechar abas à direita"), parent);
    connect(a, &QAction::triggered, this, [this]() {
        QTabWidget* tw = resolveGroup(nullptr);
        if (tw) closeToRight(tw, tw->currentIndex());
    });
    return a;
}

QAction* TabExtras::makeCloseToLeftAction(QObject* parent) {
    auto* a = new QAction(tr("Fechar abas à esquerda"), parent);
    connect(a, &QAction::triggered, this, [this]() {
        QTabWidget* tw = resolveGroup(nullptr);
        if (tw) closeToLeft(tw, tw->currentIndex());
    });
    return a;
}
