#include "ComparePanel.h"

#include "ScintillaEdit.h"
#include "ScintillaTypes.h"

#include <QDialog>
#include <QSplitter>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QWidget>
#include <QPushButton>
#include <QByteArray>
#include <QString>
#include <QStringList>

#include <vector>
#include <string>
#include <algorithm>

// ---------------------------------------------------------------------------
// Scintilla constants we use here. They are part of the stable Scintilla
// public ABI; we redefine locally to avoid pulling extra Scintilla headers.
// ---------------------------------------------------------------------------
#ifndef SC_CP_UTF8
#define SC_CP_UTF8         65001
#endif
#ifndef SC_MARGIN_NUMBER
#define SC_MARGIN_NUMBER   1
#endif
#ifndef SC_MARK_BACKGROUND
#define SC_MARK_BACKGROUND 22
#endif

// Marker numbers used by this panel
namespace {
constexpr int kMarkerAdded    = 0;
constexpr int kMarkerRemoved  = 1;
constexpr int kMarkerModified = 2;

// Scintilla expects colors in 0xBBGGRR packed order.
constexpr long packBGR(int r, int g, int b)
{
    return (long)((b << 16) | (g << 8) | r);
}

// #DCFCE7 (light green)  -> r=0xDC g=0xFC b=0xE7
const long kColorAdded    = packBGR(0xDC, 0xFC, 0xE7);
// #FEE2E2 (light red)    -> r=0xFE g=0xE2 b=0xE2
const long kColorRemoved  = packBGR(0xFE, 0xE2, 0xE2);
// #FEF9C3 (light yellow) -> r=0xFE g=0xF9 b=0xC3
const long kColorModified = packBGR(0xFE, 0xF9, 0xC3);

constexpr int kMaxLinesForLcs = 5000;

// Split UTF-8 string into lines. We keep splitting on '\n' and strip any
// trailing '\r' so that CRLF and LF are treated identically. We do not
// retain the line terminators.
std::vector<std::string> splitLines(const QString& utf8Text)
{
    const QByteArray ba = utf8Text.toUtf8();
    std::vector<std::string> out;
    out.reserve(256);

    const char* data = ba.constData();
    const int   size = ba.size();
    int start = 0;
    for (int i = 0; i < size; ++i) {
        if (data[i] == '\n') {
            int end = i;
            if (end > start && data[end - 1] == '\r') --end;
            out.emplace_back(data + start, data + end);
            start = i + 1;
        }
    }
    // trailing line (only if non-empty, to avoid spurious blank line for
    // texts that end in '\n')
    if (start < size) {
        int end = size;
        if (end > start && data[end - 1] == '\r') --end;
        out.emplace_back(data + start, data + end);
    }
    return out;
}
} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
ComparePanel::ComparePanel(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Compare"));
    resize(1100, 700);

    // Each side: a vertical layout with the title label above the editor.
    QWidget* leftSide  = new QWidget(this);
    QWidget* rightSide = new QWidget(this);

    auto* leftLayout  = new QVBoxLayout(leftSide);
    auto* rightLayout = new QVBoxLayout(rightSide);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(2);
    rightLayout->setSpacing(2);

    m_leftLabel  = new QLabel(tr("(left)"),  leftSide);
    m_rightLabel = new QLabel(tr("(right)"), rightSide);
    m_leftLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_rightLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_left  = new ScintillaEdit(leftSide);
    m_right = new ScintillaEdit(rightSide);

    leftLayout->addWidget(m_leftLabel);
    leftLayout->addWidget(m_left, 1);
    rightLayout->addWidget(m_rightLabel);
    rightLayout->addWidget(m_right, 1);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(leftSide);
    m_splitter->addWidget(rightSide);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);

    m_status = new QLabel(tr("Set both sides to compare"), this);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Close), &QPushButton::clicked,
            this, &QDialog::close);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_splitter, 1);
    mainLayout->addWidget(m_status);
    mainLayout->addWidget(buttons);

    configureEditor(m_left);
    configureEditor(m_right);

    // Synchronized scrolling: when either editor scrolls vertically, mirror
    // the first-visible-line on the other side. Scintilla emits updateUi
    // with VScroll bit set when the vertical scroll position changes.
    connect(m_left, &ScintillaEdit::updateUi,
            this, [this](Scintilla::Update updated) {
                if ((static_cast<int>(updated) & static_cast<int>(Scintilla::Update::VScroll)) == 0) return;
                syncScroll(m_left, m_right);
            });
    connect(m_right, &ScintillaEdit::updateUi,
            this, [this](Scintilla::Update updated) {
                if ((static_cast<int>(updated) & static_cast<int>(Scintilla::Update::VScroll)) == 0) return;
                syncScroll(m_right, m_left);
            });
}

// ---------------------------------------------------------------------------
// Editor setup
// ---------------------------------------------------------------------------
void ComparePanel::configureEditor(ScintillaEdit* ed)
{
    if (!ed) return;

    ed->setCodePage(SC_CP_UTF8);

    // Line numbers in margin 0
    ed->setMarginTypeN(0, SC_MARGIN_NUMBER);
    const int lnWidth = static_cast<int>(ed->textWidth(33 /*STYLE_LINENUMBER*/, "_9999"));
    ed->setMarginWidthN(0, lnWidth);

    // Hide other margins to keep the diff visually clean
    ed->setMarginWidthN(1, 0);
    ed->setMarginWidthN(2, 0);

    // Markers: SC_MARK_BACKGROUND fills the whole line with the marker color.
    ed->markerDefine(kMarkerAdded,    SC_MARK_BACKGROUND);
    ed->markerDefine(kMarkerRemoved,  SC_MARK_BACKGROUND);
    ed->markerDefine(kMarkerModified, SC_MARK_BACKGROUND);
    ed->markerSetBack(kMarkerAdded,    kColorAdded);
    ed->markerSetBack(kMarkerRemoved,  kColorRemoved);
    ed->markerSetBack(kMarkerModified, kColorModified);

    // Read-only by default — Compare is not an editing surface.
    ed->setReadOnly(true);
}

void ComparePanel::clearAllMarkers(ScintillaEdit* ed)
{
    if (!ed) return;
    ed->markerDeleteAll(kMarkerAdded);
    ed->markerDeleteAll(kMarkerRemoved);
    ed->markerDeleteAll(kMarkerModified);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void ComparePanel::setLeft(const QString& title, const QString& utf8Text)
{
    if (m_leftLabel) m_leftLabel->setText(title);

    if (m_left) {
        m_left->setReadOnly(false);
        m_left->setText(utf8Text.toUtf8().constData());
        m_left->setReadOnly(true);
        clearAllMarkers(m_left);
    }
    m_hasLeft = true;

    if (m_hasLeft && m_hasRight) runDiff();
    else if (m_status)           m_status->setText(tr("Set both sides to compare"));
}

void ComparePanel::setRight(const QString& title, const QString& utf8Text)
{
    if (m_rightLabel) m_rightLabel->setText(title);

    if (m_right) {
        m_right->setReadOnly(false);
        m_right->setText(utf8Text.toUtf8().constData());
        m_right->setReadOnly(true);
        clearAllMarkers(m_right);
    }
    m_hasRight = true;

    if (m_hasLeft && m_hasRight) runDiff();
    else if (m_status)           m_status->setText(tr("Set both sides to compare"));
}

// ---------------------------------------------------------------------------
// LCS-based line diff
// ---------------------------------------------------------------------------
void ComparePanel::runDiff()
{
    if (!m_left || !m_right) return;

    clearAllMarkers(m_left);
    clearAllMarkers(m_right);

    if (!m_hasLeft || !m_hasRight) {
        if (m_status) m_status->setText(tr("Set both sides to compare"));
        return;
    }

    // Pull current text out of both editors as UTF-8 (Scintilla's native form
    // when codepage is SC_CP_UTF8).
    const QByteArray leftBa  = m_left->getText(m_left->length() + 1);
    const QByteArray rightBa = m_right->getText(m_right->length() + 1);

    const std::vector<std::string> A = splitLines(QString::fromUtf8(leftBa));
    const std::vector<std::string> B = splitLines(QString::fromUtf8(rightBa));

    const int n = static_cast<int>(A.size());
    const int m = static_cast<int>(B.size());

    // Cap: refuse to build the LCS table for very large inputs (it's O(n*m)
    // memory and time). 5000x5000 -> ~25M ints -> ~100MB; we bail above that.
    if (n > kMaxLinesForLcs || m > kMaxLinesForLcs) {
        if (m_status) m_status->setText(tr("Diff too large to compare line-by-line"));
        return;
    }

    // Build LCS DP table.
    // dp[i][j] = length of LCS of A[0..i) and B[0..j).
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
    for (int i = 1; i <= n; ++i) {
        const std::string& ai = A[i - 1];
        for (int j = 1; j <= m; ++j) {
            if (ai == B[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }

    // Backtrack to produce edit operations as a sequence in forward order.
    // We classify each emitted op as Same / Added (right only) / Removed
    // (left only). Modified is derived in a second pass below.
    struct Op {
        LineKind kind;
        int      leftLine;   // -1 if not applicable
        int      rightLine;  // -1 if not applicable
    };
    std::vector<Op> ops;
    ops.reserve(static_cast<size_t>(n + m));
    {
        int i = n, j = m;
        std::vector<Op> rev;
        rev.reserve(static_cast<size_t>(n + m));
        while (i > 0 && j > 0) {
            if (A[i - 1] == B[j - 1]) {
                rev.push_back({Same, i - 1, j - 1});
                --i; --j;
            } else if (dp[i - 1][j] >= dp[i][j - 1]) {
                rev.push_back({Removed, i - 1, -1});
                --i;
            } else {
                rev.push_back({Added, -1, j - 1});
                --j;
            }
        }
        while (i > 0) { rev.push_back({Removed, i - 1, -1}); --i; }
        while (j > 0) { rev.push_back({Added,   -1, j - 1}); --j; }

        for (auto it = rev.rbegin(); it != rev.rend(); ++it) ops.push_back(*it);
    }

    // Pass 2: detect Modified blocks. A run of consecutive Removed ops
    // immediately followed by a run of consecutive Added ops, where both
    // runs are the same length, is treated as a Modified block: left and
    // right lines pair up 1:1 and both get the yellow marker.
    int addedCount = 0, removedCount = 0, modifiedCount = 0;

    const size_t total = ops.size();
    for (size_t k = 0; k < total; ) {
        if (ops[k].kind == Removed) {
            size_t r0 = k;
            while (k < total && ops[k].kind == Removed) ++k;
            size_t a0 = k;
            while (k < total && ops[k].kind == Added) ++k;
            const size_t rLen = a0 - r0;
            const size_t aLen = k - a0;

            if (rLen > 0 && aLen > 0 && rLen == aLen) {
                // Treat as Modified pair-by-pair.
                for (size_t t = 0; t < rLen; ++t) {
                    const int leftLn  = ops[r0 + t].leftLine;
                    const int rightLn = ops[a0 + t].rightLine;
                    if (leftLn  >= 0) m_left ->markerAdd(leftLn,  kMarkerModified);
                    if (rightLn >= 0) m_right->markerAdd(rightLn, kMarkerModified);
                    ++modifiedCount;
                }
            } else {
                for (size_t t = r0; t < a0; ++t) {
                    if (ops[t].leftLine >= 0)
                        m_left->markerAdd(ops[t].leftLine, kMarkerRemoved);
                    ++removedCount;
                }
                for (size_t t = a0; t < k; ++t) {
                    if (ops[t].rightLine >= 0)
                        m_right->markerAdd(ops[t].rightLine, kMarkerAdded);
                    ++addedCount;
                }
            }
        } else if (ops[k].kind == Added) {
            // Standalone Added run (no preceding Removed run).
            while (k < total && ops[k].kind == Added) {
                if (ops[k].rightLine >= 0)
                    m_right->markerAdd(ops[k].rightLine, kMarkerAdded);
                ++addedCount;
                ++k;
            }
        } else {
            // Same — no marker.
            ++k;
        }
    }

    updateStatus(addedCount, removedCount, modifiedCount);
}

void ComparePanel::updateStatus(int added, int removed, int modified)
{
    if (!m_status) return;
    m_status->setText(tr("%1 added, %2 removed, %3 modified")
                          .arg(added).arg(removed).arg(modified));
}

// ---------------------------------------------------------------------------
// Synchronized scrolling
// ---------------------------------------------------------------------------
void ComparePanel::syncScroll(ScintillaEdit* from, ScintillaEdit* to)
{
    if (!from || !to) return;
    if (m_syncing) return;
    m_syncing = true;
    const long firstVisible = static_cast<long>(from->firstVisibleLine());
    if (static_cast<long>(to->firstVisibleLine()) != firstVisible) {
        to->setFirstVisibleLine(firstVisible);
    }
    m_syncing = false;
}
