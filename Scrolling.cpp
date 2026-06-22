#include "Scrolling.hpp"

#include <algorithm>
#include <cmath>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/helpers/time/Timer.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/layout/target/Target.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/macros.hpp>

#include <hyprutils/string/VarList.hpp>
#include <hyprutils/string/ConstVarList.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::String;
using namespace Hyprutils::Utils;

constexpr float MIN_COLUMN_WIDTH = 0.05F;
constexpr float MAX_COLUMN_WIDTH = 1.F;
constexpr float MIN_ROW_HEIGHT   = 0.1F;
constexpr float MAX_ROW_HEIGHT   = 1.F;

//
void SColumnData::add(SP<Layout::ITarget> t) {
    for (auto& wd : windowDatas) {
        wd->windowSize *= (float)windowDatas.size() / (float)(windowDatas.size() + 1);
    }

    windowDatas.emplace_back(makeShared<SScrollingWindowData>(t, self.lock(), 1.F / (float)(windowDatas.size() + 1)));
}

void SColumnData::add(SP<Layout::ITarget> t, int after) {
    for (auto& wd : windowDatas) {
        wd->windowSize *= (float)windowDatas.size() / (float)(windowDatas.size() + 1);
    }

    windowDatas.insert(windowDatas.begin() + after + 1, makeShared<SScrollingWindowData>(t, self.lock(), 1.F / (float)(windowDatas.size() + 1)));
}

void SColumnData::add(SP<SScrollingWindowData> w) {
    for (auto& wd : windowDatas) {
        wd->windowSize *= (float)windowDatas.size() / (float)(windowDatas.size() + 1);
    }

    windowDatas.emplace_back(w);
    w->column     = self;
    w->windowSize = 1.F / (float)(windowDatas.size());
}

void SColumnData::add(SP<SScrollingWindowData> w, int after) {
    for (auto& wd : windowDatas) {
        wd->windowSize *= (float)windowDatas.size() / (float)(windowDatas.size() + 1);
    }

    windowDatas.insert(windowDatas.begin() + after + 1, w);
    w->column     = self;
    w->windowSize = 1.F / (float)(windowDatas.size());
}

size_t SColumnData::idxForHeight(float y) {
    for (size_t i = 0; i < windowDatas.size(); ++i) {
        if (windowDatas[i]->layoutBox.y < y)
            continue;
        return i > 0 ? i - 1 : 0;
    }
    return windowDatas.size() > 0 ? windowDatas.size() - 1 : 0;
}

void SColumnData::remove(SP<Layout::ITarget> t) {
    const auto SIZE_BEFORE = windowDatas.size();
    std::erase_if(windowDatas, [&t](const auto& e) { return e->target.lock() == t; });

    if (SIZE_BEFORE == windowDatas.size() && SIZE_BEFORE > 0)
        return;

    float newMaxSize = 0.F;
    for (auto& wd : windowDatas) {
        newMaxSize += wd->windowSize;
    }

    for (auto& wd : windowDatas) {
        wd->windowSize *= 1.F / newMaxSize;
    }
}

void SColumnData::up(SP<SScrollingWindowData> w) {
    for (size_t i = 1; i < windowDatas.size(); ++i) {
        if (windowDatas[i] != w)
            continue;

        std::swap(windowDatas[i], windowDatas[i - 1]);
        break;
    }
}

void SColumnData::down(SP<SScrollingWindowData> w) {
    for (size_t i = 0; i < windowDatas.size() - 1; ++i) {
        if (windowDatas[i] != w)
            continue;

        std::swap(windowDatas[i], windowDatas[i + 1]);
        break;
    }
}

SP<SScrollingWindowData> SColumnData::next(SP<SScrollingWindowData> w) {
    for (size_t i = 0; i < windowDatas.size() - 1; ++i) {
        if (windowDatas[i] != w)
            continue;

        return windowDatas[i + 1];
    }

    return nullptr;
}

SP<SScrollingWindowData> SColumnData::prev(SP<SScrollingWindowData> w) {
    for (size_t i = 1; i < windowDatas.size(); ++i) {
        if (windowDatas[i] != w)
            continue;

        return windowDatas[i - 1];
    }

    return nullptr;
}

bool SColumnData::has(SP<Layout::ITarget> t) {
    return std::ranges::find_if(windowDatas, [t](const auto& e) { return e->target.lock() == t; }) != windowDatas.end();
}

SP<SColumnData> SScrollingLayoutData::add() {
    auto col       = columns.emplace_back(makeShared<SColumnData>(layout));
    col->self      = col;
    col->columnWidth = layout->defaultColumnWidth();
    return col;
}

SP<SColumnData> SScrollingLayoutData::add(int after) {
    auto col       = makeShared<SColumnData>(layout);
    col->self      = col;
    col->columnWidth = layout->defaultColumnWidth();
    columns.insert(columns.begin() + after + 1, col);
    return col;
}

int64_t SScrollingLayoutData::idx(SP<SColumnData> c) {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i] == c)
            return i;
    }

    return -1;
}

void SScrollingLayoutData::remove(SP<SColumnData> c) {
    std::erase(columns, c);
}

SP<SColumnData> SScrollingLayoutData::next(SP<SColumnData> c) {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i] != c)
            continue;

        if (i == columns.size() - 1)
            return nullptr;

        return columns[i + 1];
    }

    return nullptr;
}

SP<SColumnData> SScrollingLayoutData::prev(SP<SColumnData> c) {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i] != c)
            continue;

        if (i == 0)
            return nullptr;

        return columns[i - 1];
    }

    return nullptr;
}

void SScrollingLayoutData::centerCol(SP<SColumnData> c) {
    if (!c)
        return;

    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:fullscreen_on_one_column");

    const auto        USABLE      = layout->usableArea();
    double            currentLeft = 0;

    for (const auto& COL : columns) {
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        if (COL != c)
            currentLeft += ITEM_WIDTH;
        else {
            leftOffset = currentLeft - (USABLE.w - ITEM_WIDTH) / 2.F;
            return;
        }
    }
}

void SScrollingLayoutData::fitCol(SP<SColumnData> c) {
    if (!c)
        return;

    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:fullscreen_on_one_column");

    const auto        USABLE      = layout->usableArea();
    double            currentLeft = 0;

    for (const auto& COL : columns) {
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        if (COL != c)
            currentLeft += ITEM_WIDTH;
        else {
            leftOffset = std::clamp((double)leftOffset, currentLeft - USABLE.w + ITEM_WIDTH, currentLeft);
            return;
        }
    }
}

void SScrollingLayoutData::centerOrFitCol(SP<SColumnData> c) {
    if (!c)
        return;

    static const auto PFITMETHOD = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:focus_fit_method");

    if (*PFITMETHOD == 1)
        fitCol(c);
    else
        centerCol(c);
}

SP<SColumnData> SScrollingLayoutData::atCenter() {
    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:fullscreen_on_one_column");

    double            currentLeft = leftOffset;
    const auto        USABLE      = layout->usableArea();

    for (const auto& COL : columns) {
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        currentLeft += ITEM_WIDTH;

        if (currentLeft >= USABLE.w / 2.0 - 2)
            return COL;
    }

    return nullptr;
}

void SScrollingLayoutData::recalculate(bool forceInstant) {
    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:fullscreen_on_one_column");
    static const auto PCOLLAPSED_W = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:collapsed_width");

    auto parent = layout->m_parent.lock();
    if (!parent || !parent->space()) {
        return;
    }

    const CBox   USABLE    = layout->usableArea();
    const auto   WORKAREA  = parent->space()->workArea();

    // Zen mode: only show the zen column (or focused column)
    if (layout->m_zenMode) {
        SP<SColumnData> zenCol = layout->m_zenColumn;
        if (!zenCol || zenCol->windowDatas.empty()) {
            layout->m_zenMode = false;
            layout->m_zenColumn = nullptr;
        } else {
            // Render only the zen column at full width
            double currentTop = 0.0;
            for (const auto& WDATA : zenCol->windowDatas) {
                WDATA->layoutBox = CBox{0, currentTop, USABLE.w, WDATA->windowSize * USABLE.h}
                                       .translate(WORKAREA.pos());
                currentTop += WDATA->windowSize * USABLE.h;

                auto target = WDATA->target.lock();
                if (target) {
                    CBox box = WDATA->layoutBox;
                    box.round();
                    target->setPositionGlobal(box);
                    if (forceInstant) { target->warpPositionSize(); target->damageEntire(); }
                }
            }

            // Move all other columns off-screen
            for (const auto& COL : columns) {
                if (COL == zenCol)
                    continue;
                for (const auto& WDATA : COL->windowDatas) {
                    WDATA->layoutBox = CBox{-9999, -9999, 1, 1};
                    auto target = WDATA->target.lock();
                    if (target) {
                        target->setPositionGlobal(WDATA->layoutBox);
                        if (forceInstant) { target->warpPositionSize(); target->damageEntire(); }
                    }
                }
            }
            return;
        }
    }

    const double PINNED_L  = pinnedWidthLeft();
    const double PINNED_R  = pinnedWidthRight();
    const double SCROLL_W  = USABLE.w - PINNED_L - PINNED_R;
    const double COLLAPSED_PX = *PCOLLAPSED_W;

    // Count scrollable columns and their total width (accounting for collapsed)
    double scrollableMaxWidth = 0;
    for (const auto& COL : columns) {
        if (COL->pinned != PIN_NONE)
            continue;
        if (COL->collapsed) {
            scrollableMaxWidth += COLLAPSED_PX;
        } else {
            const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;
            scrollableMaxWidth += ITEM_WIDTH;
        }
    }

    const double cameraLeft = scrollableMaxWidth < SCROLL_W ? std::round((scrollableMaxWidth - SCROLL_W) / 2.0) : leftOffset;

    // Render pinned-left columns first
    double pinnedLeftX = 0;
    for (const auto& COL : columns) {
        if (COL->pinned != PIN_LEFT)
            continue;

        const double ITEM_WIDTH = COL->collapsed ? COLLAPSED_PX : USABLE.w * COL->columnWidth;
        double       currentTop = 0.0;

        for (const auto& WDATA : COL->windowDatas) {
            WDATA->layoutBox = CBox{pinnedLeftX, currentTop, ITEM_WIDTH, WDATA->windowSize * USABLE.h}
                                   .translate(WORKAREA.pos());
            currentTop += WDATA->windowSize * USABLE.h;

            auto target = WDATA->target.lock();
            if (target) {
                CBox box = WDATA->layoutBox;
                box.round();
                target->setPositionGlobal(box);
                if (forceInstant) { target->warpPositionSize(); target->damageEntire(); }
            }
        }
        pinnedLeftX += ITEM_WIDTH;
    }

    // Render pinned-right columns
    double pinnedRightX = USABLE.w;
    for (int64_t i = (int64_t)columns.size() - 1; i >= 0; --i) {
        const auto& COL = columns[i];
        if (COL->pinned != PIN_RIGHT)
            continue;

        const double ITEM_WIDTH = COL->collapsed ? COLLAPSED_PX : USABLE.w * COL->columnWidth;
        pinnedRightX -= ITEM_WIDTH;
        double currentTop = 0.0;

        for (const auto& WDATA : COL->windowDatas) {
            WDATA->layoutBox = CBox{pinnedRightX, currentTop, ITEM_WIDTH, WDATA->windowSize * USABLE.h}
                                   .translate(WORKAREA.pos());
            currentTop += WDATA->windowSize * USABLE.h;

            auto target = WDATA->target.lock();
            if (target) {
                CBox box = WDATA->layoutBox;
                box.round();
                target->setPositionGlobal(box);
                if (forceInstant) { target->warpPositionSize(); target->damageEntire(); }
            }
        }
    }

    // Render scrollable (unpinned) columns
    double currentLeft = 0;
    for (size_t i = 0; i < columns.size(); ++i) {
        const auto&  COL = columns[i];
        if (COL->pinned != PIN_NONE)
            continue;

        double       currentTop = 0.0;
        const double ITEM_WIDTH = COL->collapsed ? COLLAPSED_PX :
                                  (*PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth);

        for (const auto& WDATA : COL->windowDatas) {
            WDATA->layoutBox = CBox{currentLeft, currentTop, ITEM_WIDTH, WDATA->windowSize * USABLE.h}
                                   .translate(WORKAREA.pos() + Vector2D{PINNED_L - cameraLeft, 0.0});

            currentTop += WDATA->windowSize * USABLE.h;

            auto target = WDATA->target.lock();
            if (target) {
                CBox box = WDATA->layoutBox;
                box.round();
                target->setPositionGlobal(box);

                if (forceInstant) {
                    target->warpPositionSize();
                    target->damageEntire();
                }
            }
        }

        currentLeft += ITEM_WIDTH;
        if (std::abs(currentLeft - SCROLL_W) < 0.5)
            currentLeft++;
    }
}

double SScrollingLayoutData::maxWidth() {
    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:fullscreen_on_one_column");

    double            currentLeft = 0;
    const auto        USABLE      = layout->usableArea();

    for (const auto& COL : columns) {
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;
        currentLeft += ITEM_WIDTH;
    }

    return currentLeft;
}

double SScrollingLayoutData::pinnedWidthLeft() {
    const auto USABLE = layout->usableArea();
    double     w      = 0;
    for (const auto& COL : columns) {
        if (COL->pinned == PIN_LEFT)
            w += USABLE.w * COL->columnWidth;
    }
    return w;
}

double SScrollingLayoutData::pinnedWidthRight() {
    const auto USABLE = layout->usableArea();
    double     w      = 0;
    for (const auto& COL : columns) {
        if (COL->pinned == PIN_RIGHT)
            w += USABLE.w * COL->columnWidth;
    }
    return w;
}

bool SScrollingLayoutData::visible(SP<SColumnData> c) {
    const auto USABLE    = layout->usableArea();
    float      totalLeft = 0;
    for (const auto& col : columns) {
        if (col == c) {
            const float colLeft   = totalLeft;
            const float colRight  = totalLeft + col->columnWidth * USABLE.w;
            const float viewLeft  = leftOffset;
            const float viewRight = leftOffset + USABLE.w;
            return colLeft < viewRight && viewLeft < colRight;
        }

        totalLeft += col->columnWidth * USABLE.w;
    }

    return false;
}

// ======================
// CScrollingLayout
// ======================

CScrollingLayout::CScrollingLayout() {
    m_scrollingData = makeShared<SScrollingLayoutData>(this);
    m_scrollingData->self = m_scrollingData;
}

CScrollingLayout::~CScrollingLayout() {
    m_configCallback.reset();
    m_focusCallback.reset();
}

float CScrollingLayout::defaultColumnWidth() {
    static const auto PCOLWIDTH = CConfigValue<Hyprlang::FLOAT>("plugin:hyprscrolling:column_width");
    return *PCOLWIDTH;
}

CBox CScrollingLayout::usableArea() {
    auto parent = m_parent.lock();
    if (!parent || !parent->space())
        return {};

    const auto WORKAREA = parent->space()->workArea();
    auto       ws       = parent->space()->workspace();
    if (!ws)
        return {};

    auto PMONITOR = ws->m_monitor.lock();
    if (!PMONITOR)
        return {};

    // Work area is in global coords; make it relative to monitor
    CBox result = WORKAREA;
    result.translate(-PMONITOR->m_position);
    return result;
}

void CScrollingLayout::newTarget(SP<Layout::ITarget> target) {
    if (!target)
        return;

    // Initialize config callback on first use
    if (!m_configCallback) {
        static const auto PCONFWIDTHS = CConfigValue<Hyprlang::STRING>("plugin:hyprscrolling:explicit_column_widths");

        m_configCallback = Event::bus()->m_events.config.reloaded.listen([this]() {
            m_config.configuredWidths.clear();

            static const auto PCONFWIDTHS = CConfigValue<Hyprlang::STRING>("plugin:hyprscrolling:explicit_column_widths");

            CConstVarList widths(*PCONFWIDTHS, 0, ',');
            for (auto& w : widths) {
                try {
                    m_config.configuredWidths.emplace_back(std::stof(std::string{w}));
                } catch (...) { ; }
            }
            if (m_config.configuredWidths.empty())
                m_config.configuredWidths = {0.333, 0.5, 0.667, 1.0};

            // Parse auto width rules: "class1:0.7,class2:0.3"
            static const auto PAUTOWIDTHS = CConfigValue<Hyprlang::STRING>("plugin:hyprscrolling:auto_width_rules");
            m_config.autoWidthRules.clear();
            CConstVarList rules(*PAUTOWIDTHS, 0, ',');
            for (auto& r : rules) {
                std::string rule{r};
                auto pos = rule.find(':');
                if (pos != std::string::npos) {
                    std::string cls = rule.substr(0, pos);
                    // trim whitespace
                    while (!cls.empty() && cls.front() == ' ') cls.erase(cls.begin());
                    while (!cls.empty() && cls.back() == ' ') cls.pop_back();
                    try {
                        float val = std::stof(rule.substr(pos + 1));
                        m_config.autoWidthRules[cls] = val;
                    } catch (...) { ; }
                }
            }
        });

        // trigger initial parse
        m_config.configuredWidths.clear();
        CConstVarList widths(*PCONFWIDTHS, 0, ',');
        for (auto& w : widths) {
            try {
                m_config.configuredWidths.emplace_back(std::stof(std::string{w}));
            } catch (...) { ; }
        }
        if (m_config.configuredWidths.empty())
            m_config.configuredWidths = {0.333, 0.5, 0.667, 1.0};

        // Parse auto width rules initially
        static const auto PAUTOWIDTHS = CConfigValue<Hyprlang::STRING>("plugin:hyprscrolling:auto_width_rules");
        m_config.autoWidthRules.clear();
        CConstVarList rules(*PAUTOWIDTHS, 0, ',');
        for (auto& r : rules) {
            std::string rule{r};
            auto pos = rule.find(':');
            if (pos != std::string::npos) {
                std::string cls = rule.substr(0, pos);
                while (!cls.empty() && cls.front() == ' ') cls.erase(cls.begin());
                while (!cls.empty() && cls.back() == ' ') cls.pop_back();
                try {
                    float val = std::stof(rule.substr(pos + 1));
                    m_config.autoWidthRules[cls] = val;
                } catch (...) { ; }
            }
        }
        if (m_config.autoWidthRules.empty()) { ; } // just to avoid unused warning
    }

    if (!m_focusCallback) {
        m_focusCallback = Event::bus()->m_events.window.active.listen([this](PHLWINDOW pWindow, Desktop::eFocusReason reason) {
            if (!pWindow)
                return;

            static const auto PFOLLOW_FOCUS = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:follow_focus");

            if (!*PFOLLOW_FOCUS)
                return;

            // Find the target for this window
            auto parent = m_parent.lock();
            if (!parent || !parent->space())
                return;

            // Check this window belongs to our space
            auto ws = parent->space()->workspace();
            if (!ws || pWindow->m_workspace != ws)
                return;

            // Find the target
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (!t || t->window() != pWindow)
                    continue;

                auto WDATA = dataFor(t);
                if (!WDATA)
                    break;

                static const auto PFOLLOW_DEBOUNCE_MS = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:follow_debounce_ms");
                static CTimer     debounceTimer;
                if (debounceTimer.getMillis() < *PFOLLOW_DEBOUNCE_MS)
                    return;

                static const auto PFITMETHOD = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:focus_fit_method");
                if (*PFITMETHOD == 1)
                    m_scrollingData->fitCol(WDATA->column.lock());
                else
                    m_scrollingData->centerCol(WDATA->column.lock());
                m_scrollingData->recalculate();
                debounceTimer.reset();
                break;
            }
        });
    }

    // Try to find the focused target for determining placement
    SP<Layout::ITarget> droppingOn = nullptr;
    auto                parent     = m_parent.lock();

    if (parent && parent->space()) {
        // Find the most recently focused target in this space
        for (auto& wt : parent->space()->targets()) {
            auto t = wt.lock();
            if (!t || t == target)
                continue;

            auto w = t->window();
            if (w && w == Desktop::focusState()->window()) {
                droppingOn = t;
                break;
            }
        }
    }

    SP<SScrollingWindowData> droppingData   = droppingOn ? dataFor(droppingOn) : nullptr;
    SP<SColumnData>          droppingColumn = droppingData ? droppingData->column.lock() : nullptr;

    if (!droppingColumn) {
        auto col = m_scrollingData->add();
        col->add(target);
        col->columnWidth = autoWidthForTarget(target);
        m_scrollingData->fitCol(col);
    } else {
        auto idx = m_scrollingData->idx(droppingColumn);
        auto col = idx == -1 ? m_scrollingData->add() : m_scrollingData->add(idx);
        col->add(target);
        col->columnWidth = autoWidthForTarget(target);
        m_scrollingData->fitCol(col);
    }

    m_scrollingData->recalculate();
}

void CScrollingLayout::movedTarget(SP<Layout::ITarget> target, std::optional<Vector2D> focalPoint) {
    newTarget(target);
}

void CScrollingLayout::removeTarget(SP<Layout::ITarget> target) {
    auto DATA = dataFor(target);

    if (!DATA)
        return;

    auto COL = DATA->column.lock();
    if (!COL)
        return;

    if (!m_scrollingData->next(COL)) {
        // move the view if this is the last column
        const auto USABLE = usableArea();
        m_scrollingData->leftOffset -= USABLE.w * COL->columnWidth;
    }

    COL->remove(target);

    // Remove empty columns
    if (COL->windowDatas.empty()) {
        m_scrollingData->remove(COL);
    }

    m_scrollingData->recalculate();

    // Ensure we don't leave extra space
    const auto USABLE = usableArea();
    m_scrollingData->leftOffset = std::clamp((double)m_scrollingData->leftOffset, 0.0,
        std::max(m_scrollingData->maxWidth() - USABLE.w, 0.0));
}

void CScrollingLayout::recalculate(Layout::eRecalculateReason reason) {
    m_scrollingData->recalculate();
}

void CScrollingLayout::resizeTarget(const Vector2D& delta, SP<Layout::ITarget> target, Layout::eRectCorner corner) {
    if (!target)
        return;

    const auto DATA = dataFor(target);

    if (!DATA || !DATA->column.lock())
        return;

    const auto USABLE        = usableArea();
    const auto DELTA_AS_PERC = delta / USABLE.size();

    const auto CURR_COLUMN = DATA->column.lock();
    const auto NEXT_COLUMN = m_scrollingData->next(CURR_COLUMN);
    const auto PREV_COLUMN = m_scrollingData->prev(CURR_COLUMN);

    switch (corner) {
        case Layout::CORNER_BOTTOMLEFT:
        case Layout::CORNER_TOPLEFT: {
            if (!PREV_COLUMN)
                break;

            PREV_COLUMN->columnWidth = std::clamp(PREV_COLUMN->columnWidth + (float)DELTA_AS_PERC.x, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            CURR_COLUMN->columnWidth = std::clamp(CURR_COLUMN->columnWidth - (float)DELTA_AS_PERC.x, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            break;
        }
        case Layout::CORNER_BOTTOMRIGHT:
        case Layout::CORNER_TOPRIGHT: {
            if (!NEXT_COLUMN)
                break;

            NEXT_COLUMN->columnWidth = std::clamp(NEXT_COLUMN->columnWidth - (float)DELTA_AS_PERC.x, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            CURR_COLUMN->columnWidth = std::clamp(CURR_COLUMN->columnWidth + (float)DELTA_AS_PERC.x, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            break;
        }

        default: break;
    }

    Vector2D modDelta = delta;

    if (DATA->column.lock()->windowDatas.size() > 1) {
        const auto CURR_WD = DATA;
        const auto NEXT_WD = DATA->column.lock()->next(DATA);
        const auto PREV_WD = DATA->column.lock()->prev(DATA);

        auto effectiveCorner = corner;
        if (effectiveCorner == Layout::CORNER_NONE) {
            if (!PREV_WD)
                effectiveCorner = Layout::CORNER_BOTTOMRIGHT;
            else {
                effectiveCorner = Layout::CORNER_TOPRIGHT;
                modDelta.y *= -1.0f;
            }
        }

        switch (effectiveCorner) {
            case Layout::CORNER_BOTTOMLEFT:
            case Layout::CORNER_BOTTOMRIGHT: {
                if (!NEXT_WD)
                    break;

                if (NEXT_WD->windowSize <= MIN_ROW_HEIGHT && delta.y >= 0)
                    break;

                float adjust = std::clamp((float)(delta.y / USABLE.h), (-CURR_WD->windowSize + MIN_ROW_HEIGHT), (NEXT_WD->windowSize - MIN_ROW_HEIGHT));

                NEXT_WD->windowSize = std::clamp(NEXT_WD->windowSize - adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                CURR_WD->windowSize = std::clamp(CURR_WD->windowSize + adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                break;
            }
            case Layout::CORNER_TOPLEFT:
            case Layout::CORNER_TOPRIGHT: {
                if (!PREV_WD)
                    break;

                if ((PREV_WD->windowSize <= MIN_ROW_HEIGHT && modDelta.y <= 0) || (CURR_WD->windowSize <= MIN_ROW_HEIGHT && delta.y >= 0))
                    break;

                float adjust = std::clamp((float)(modDelta.y / USABLE.h), -(PREV_WD->windowSize - MIN_ROW_HEIGHT), (CURR_WD->windowSize - MIN_ROW_HEIGHT));

                PREV_WD->windowSize = std::clamp(PREV_WD->windowSize + adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                CURR_WD->windowSize = std::clamp(CURR_WD->windowSize - adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                break;
            }

            default: break;
        }
    }

    m_scrollingData->recalculate(true);
}

SP<Layout::ITarget> CScrollingLayout::getNextCandidate(SP<Layout::ITarget> old) {
    auto DATA = dataFor(old);
    if (!DATA)
        return nullptr;

    auto COL = DATA->column.lock();
    if (!COL)
        return nullptr;

    // Try next/prev in same column
    auto NEXT = COL->next(DATA);
    if (NEXT)
        return NEXT->target.lock();

    auto PREV = COL->prev(DATA);
    if (PREV)
        return PREV->target.lock();

    // Try adjacent columns
    auto NEXTCOL = m_scrollingData->next(COL);
    if (NEXTCOL && !NEXTCOL->windowDatas.empty())
        return NEXTCOL->windowDatas.front()->target.lock();

    auto PREVCOL = m_scrollingData->prev(COL);
    if (PREVCOL && !PREVCOL->windowDatas.empty())
        return PREVCOL->windowDatas.back()->target.lock();

    return nullptr;
}

void CScrollingLayout::focusTargetUpdate(SP<Layout::ITarget> target) {
    if (!target) {
        Desktop::focusState()->fullWindowFocus(PHLWINDOW{nullptr}, Desktop::FOCUS_REASON_OTHER);
        return;
    }

    auto w = target->window();
    if (!w) {
        Desktop::focusState()->fullWindowFocus(PHLWINDOW{nullptr}, Desktop::FOCUS_REASON_OTHER);
        return;
    }

    Desktop::focusState()->fullWindowFocus(w, Desktop::FOCUS_REASON_OTHER);

    const auto WDATA = dataFor(target);
    if (WDATA) {
        if (auto col = WDATA->column.lock())
            col->lastFocusedWindow = WDATA;
    }

    pushFocusHistory(target);
}

SP<SScrollingWindowData> CScrollingLayout::findBestNeighbor(SP<SScrollingWindowData> pCurrent, SP<SColumnData> pTargetCol) {
    if (!pCurrent || !pTargetCol || pTargetCol->windowDatas.empty())
        return nullptr;

    const double                          currentTop    = pCurrent->layoutBox.y;
    const double                          currentBottom = pCurrent->layoutBox.y + pCurrent->layoutBox.h;
    std::vector<SP<SScrollingWindowData>> overlappingWindows;
    for (const auto& candidate : pTargetCol->windowDatas) {
        const double candidateTop    = candidate->layoutBox.y;
        const double candidateBottom = candidate->layoutBox.y + candidate->layoutBox.h;
        const bool   overlaps        = (candidateTop < currentBottom) && (candidateBottom > currentTop);

        if (overlaps)
            overlappingWindows.emplace_back(candidate);
    }
    if (!overlappingWindows.empty()) {
        auto lastFocused = pTargetCol->lastFocusedWindow.lock();

        if (lastFocused) {
            auto it = std::ranges::find(overlappingWindows, lastFocused);
            if (it != overlappingWindows.end())
                return lastFocused;
        }

        auto topmost = std::ranges::min_element(overlappingWindows, std::less<>{}, [](const SP<SScrollingWindowData>& w) { return w->layoutBox.y; });
        return *topmost;
    }
    if (!pTargetCol->windowDatas.empty())
        return pTargetCol->windowDatas.front();
    return nullptr;
}

Config::ErrorResult CScrollingLayout::layoutMsg(const std::string_view& sv) {
    const std::string message{sv};

    static auto centerOrFit = [](const SP<SScrollingLayoutData> WS, const SP<SColumnData> COL) -> void {
        static const auto PFITMETHOD = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:focus_fit_method");
        if (*PFITMETHOD == 1)
            WS->fitCol(COL);
        else
            WS->centerCol(COL);
    };

    const auto ARGS = CVarList(message, 0, ' ');
    if (ARGS[0] == "move") {
        if (ARGS[1] == "+col" || ARGS[1] == "col") {
            auto focusedTarget = Desktop::focusState()->window();
            if (!focusedTarget)
                return {};

            SP<SScrollingWindowData> WDATA = nullptr;
            auto                     parent = m_parent.lock();
            if (parent && parent->space()) {
                for (auto& wt : parent->space()->targets()) {
                    auto t = wt.lock();
                    if (t && t->window() == focusedTarget) {
                        WDATA = dataFor(t);
                        break;
                    }
                }
            }
            if (!WDATA)
                return {};

            const auto COL = m_scrollingData->next(WDATA->column.lock());
            if (!COL) {
                m_scrollingData->leftOffset = m_scrollingData->maxWidth();
                m_scrollingData->recalculate();
                focusTargetUpdate(nullptr);
                return {};
            }

            centerOrFit(m_scrollingData, COL);
            m_scrollingData->recalculate();

            auto target = COL->windowDatas.front()->target.lock();
            focusTargetUpdate(target);
            if (target && target->window())
                g_pCompositor->warpCursorTo(target->window()->middle());

            return {};
        } else if (ARGS[1] == "-col") {
            auto focusedTarget = Desktop::focusState()->window();

            SP<SScrollingWindowData> WDATA = nullptr;
            auto                     parent = m_parent.lock();
            if (parent && parent->space()) {
                for (auto& wt : parent->space()->targets()) {
                    auto t = wt.lock();
                    if (t && t->window() == focusedTarget) {
                        WDATA = dataFor(t);
                        break;
                    }
                }
            }

            if (!WDATA) {
                if (m_scrollingData->columns.size() > 0) {
                    m_scrollingData->centerCol(m_scrollingData->columns.back());
                    m_scrollingData->recalculate();
                    auto target = m_scrollingData->columns.back()->windowDatas.back()->target.lock();
                    focusTargetUpdate(target);
                    if (target && target->window())
                        g_pCompositor->warpCursorTo(target->window()->middle());
                }

                return {};
            }

            const auto COL = m_scrollingData->prev(WDATA->column.lock());
            if (!COL)
                return {};

            centerOrFit(m_scrollingData, COL);
            m_scrollingData->recalculate();

            auto target = COL->windowDatas.back()->target.lock();
            focusTargetUpdate(target);
            if (target && target->window())
                g_pCompositor->warpCursorTo(target->window()->middle());

            return {};
        }

        const auto PLUSMINUS = getPlusMinusKeywordResult(ARGS[1], 0);

        if (!PLUSMINUS.has_value())
            return {};

        m_scrollingData->leftOffset -= *PLUSMINUS;
        m_scrollingData->recalculate();

        const auto ATCENTER = m_scrollingData->atCenter();

        focusTargetUpdate(ATCENTER ? (*ATCENTER->windowDatas.begin())->target.lock() : nullptr);
    } else if (ARGS[0] == "colresize") {
        auto focusedWindow = Desktop::focusState()->window();
        if (!focusedWindow)
            return {};

        SP<SScrollingWindowData> WDATA = nullptr;
        auto                     parent = m_parent.lock();
        if (parent && parent->space()) {
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (t && t->window() == focusedWindow) {
                    WDATA = dataFor(t);
                    break;
                }
            }
        }

        if (!WDATA)
            return {};

        if (ARGS[1] == "all") {
            float abs = 0;
            try {
                abs = std::stof(ARGS[2]);
            } catch (...) { return {}; }

            for (const auto& c : m_scrollingData->columns) {
                c->columnWidth = abs;
            }

            m_scrollingData->recalculate();
            return {};
        }

        auto COL = WDATA->column.lock();
        CScopeGuard x([this, COL] {
            COL->columnWidth = std::clamp(COL->columnWidth, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            m_scrollingData->centerOrFitCol(COL);
            m_scrollingData->recalculate();
        });

        if (ARGS[1][0] == '+' || ARGS[1][0] == '-') {
            if (ARGS[1] == "+conf") {
                for (size_t i = 0; i < m_config.configuredWidths.size(); ++i) {
                    if (m_config.configuredWidths[i] > COL->columnWidth) {
                        COL->columnWidth = m_config.configuredWidths[i];
                        break;
                    }

                    if (i == m_config.configuredWidths.size() - 1)
                        COL->columnWidth = m_config.configuredWidths[0];
                }

                return {};
            } else if (ARGS[1] == "-conf") {
                if (m_config.configuredWidths.empty())
                    return {};

                for (size_t i = m_config.configuredWidths.size() - 1;; --i) {
                    if (m_config.configuredWidths[i] < COL->columnWidth) {
                        COL->columnWidth = m_config.configuredWidths[i];
                        break;
                    }

                    if (i == 0) {
                        COL->columnWidth = m_config.configuredWidths.back();
                        break;
                    }
                }

                return {};
            }

            const auto PLUSMINUS = getPlusMinusKeywordResult(ARGS[1], 0);

            if (!PLUSMINUS.has_value())
                return {};

            COL->columnWidth += *PLUSMINUS;
        } else {
            float abs = 0;
            try {
                abs = std::stof(ARGS[1]);
            } catch (...) { return {}; }

            COL->columnWidth = abs;
        }
    } else if (ARGS[0] == "movewindowto") {
        auto focusedWindow = Desktop::focusState()->window();
        if (!focusedWindow)
            return {};

        auto parent = m_parent.lock();
        if (parent && parent->space()) {
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (t && t->window() == focusedWindow) {
                    moveTargetTo(t, Math::fromChar(ARGS[1][0]), false);
                    break;
                }
            }
        }
    } else if (ARGS[0] == "fit") {

        auto focusedWindow = Desktop::focusState()->window();
        if (!focusedWindow)
            return {};

        SP<SScrollingWindowData> WDATA = nullptr;
        auto                     parent = m_parent.lock();
        if (parent && parent->space()) {
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (t && t->window() == focusedWindow) {
                    WDATA = dataFor(t);
                    break;
                }
            }
        }

        if (ARGS[1] == "active") {
            if (!WDATA || m_scrollingData->columns.size() == 0)
                return {};

            const auto USABLE = usableArea();

            WDATA->column.lock()->columnWidth = 1.F;

            auto target = WDATA->target.lock();
            if (!target)
                return {};

            m_scrollingData->leftOffset = 0;
            for (size_t i = 0; i < m_scrollingData->columns.size(); ++i) {
                if (m_scrollingData->columns[i]->has(target))
                    break;

                m_scrollingData->leftOffset += USABLE.w * m_scrollingData->columns[i]->columnWidth;
            }

            m_scrollingData->recalculate();
        } else if (ARGS[1] == "all") {
            if (m_scrollingData->columns.size() == 0)
                return {};

            const size_t LEN = m_scrollingData->columns.size();
            for (const auto& c : m_scrollingData->columns) {
                c->columnWidth = 1.F / (float)LEN;
            }

            m_scrollingData->recalculate();
        } else if (ARGS[1] == "toend") {
            if (m_scrollingData->columns.size() == 0)
                return {};

            auto target = WDATA ? WDATA->target.lock() : nullptr;
            if (!target)
                return {};

            bool   begun   = false;
            size_t foundAt = 0;
            for (size_t i = 0; i < m_scrollingData->columns.size(); ++i) {
                if (!begun && !m_scrollingData->columns[i]->has(target))
                    continue;

                if (!begun) {
                    begun   = true;
                    foundAt = i;
                }

                m_scrollingData->columns[i]->columnWidth = 1.F / (float)(m_scrollingData->columns.size() - i);
            }

            if (!begun)
                return {};

            const auto USABLE = usableArea();

            m_scrollingData->leftOffset = 0;
            for (size_t i = 0; i < foundAt; ++i) {
                m_scrollingData->leftOffset += USABLE.w * m_scrollingData->columns[i]->columnWidth;
            }

            m_scrollingData->recalculate();
        } else if (ARGS[1] == "tobeg") {
            if (m_scrollingData->columns.size() == 0)
                return {};

            auto target = WDATA ? WDATA->target.lock() : nullptr;
            if (!target)
                return {};

            bool   begun   = false;
            size_t foundAt = 0;
            for (int64_t i = (int64_t)m_scrollingData->columns.size() - 1; i >= 0; --i) {
                if (!begun && !m_scrollingData->columns[i]->has(target))
                    continue;

                if (!begun) {
                    begun   = true;
                    foundAt = i;
                }

                m_scrollingData->columns[i]->columnWidth = 1.F / (float)(foundAt + 1);
            }

            if (!begun)
                return {};

            m_scrollingData->leftOffset = 0;

            m_scrollingData->recalculate();
        } else if (ARGS[1] == "visible") {
            if (m_scrollingData->columns.size() == 0)
                return {};

            bool                         begun   = false;
            size_t                       foundAt = 0;
            std::vector<SP<SColumnData>> visible;
            for (size_t i = 0; i < m_scrollingData->columns.size(); ++i) {
                if (!begun && !m_scrollingData->visible(m_scrollingData->columns[i]))
                    continue;

                if (!begun) {
                    begun   = true;
                    foundAt = i;
                }

                if (!m_scrollingData->visible(m_scrollingData->columns[i]))
                    break;

                visible.emplace_back(m_scrollingData->columns[i]);
            }

            if (!begun)
                return {};

            m_scrollingData->leftOffset = 0;

            if (foundAt != 0) {
                const auto USABLE = usableArea();

                for (size_t i = 0; i < foundAt; ++i) {
                    m_scrollingData->leftOffset += USABLE.w * m_scrollingData->columns[i]->columnWidth;
                }
            }

            for (const auto& v : visible) {
                v->columnWidth = 1.F / (float)visible.size();
            }

            m_scrollingData->recalculate();
        }
    } else if (ARGS[0] == "focus") {
        auto focusedWindow = Desktop::focusState()->window();
        if (!focusedWindow)
            return {};

        SP<SScrollingWindowData> WDATA = nullptr;
        auto                     parent = m_parent.lock();
        if (parent && parent->space()) {
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (t && t->window() == focusedWindow) {
                    WDATA = dataFor(t);
                    break;
                }
            }
        }

        static const auto PNOFALLBACK = CConfigValue<Hyprlang::INT>("general:no_focus_fallback");

        if (!WDATA || ARGS[1].empty())
            return {};

        switch (ARGS[1][0]) {
            case 'u':
            case 't': {
                auto PREV = WDATA->column.lock()->prev(WDATA);
                if (!PREV) {
                    if (*PNOFALLBACK)
                        break;
                    else
                        PREV = WDATA->column.lock()->windowDatas.back();
                }

                focusTargetUpdate(PREV->target.lock());
                auto w = PREV->target.lock() ? PREV->target.lock()->window() : nullptr;
                if (w)
                    g_pCompositor->warpCursorTo(w->middle());
                break;
            }

            case 'b':
            case 'd': {
                auto NEXT = WDATA->column.lock()->next(WDATA);
                if (!NEXT) {
                    if (*PNOFALLBACK)
                        break;
                    else
                        NEXT = WDATA->column.lock()->windowDatas.front();
                }

                focusTargetUpdate(NEXT->target.lock());
                auto w = NEXT->target.lock() ? NEXT->target.lock()->window() : nullptr;
                if (w)
                    g_pCompositor->warpCursorTo(w->middle());
                break;
            }

            case 'l': {
                auto COL  = WDATA->column.lock();
                auto PREV = m_scrollingData->prev(COL);
                if (!PREV) {
                    if (*PNOFALLBACK) {
                        centerOrFit(m_scrollingData, COL);
                        m_scrollingData->recalculate();
                        auto w = WDATA->target.lock() ? WDATA->target.lock()->window() : nullptr;
                        if (w)
                            g_pCompositor->warpCursorTo(w->middle());
                        break;
                    } else
                        PREV = m_scrollingData->columns.back();
                }

                auto pTargetWindowData = findBestNeighbor(WDATA, PREV);
                if (pTargetWindowData) {
                    focusTargetUpdate(pTargetWindowData->target.lock());
                    centerOrFit(m_scrollingData, PREV);
                    m_scrollingData->recalculate();
                    auto w = pTargetWindowData->target.lock() ? pTargetWindowData->target.lock()->window() : nullptr;
                    if (w)
                        g_pCompositor->warpCursorTo(w->middle());
                }
                break;
            }

            case 'r': {
                auto COL  = WDATA->column.lock();
                auto NEXT = m_scrollingData->next(COL);
                if (!NEXT) {
                    if (*PNOFALLBACK) {
                        centerOrFit(m_scrollingData, COL);
                        m_scrollingData->recalculate();
                        auto w = WDATA->target.lock() ? WDATA->target.lock()->window() : nullptr;
                        if (w)
                            g_pCompositor->warpCursorTo(w->middle());
                        break;
                    } else
                        NEXT = m_scrollingData->columns.front();
                }

                auto pTargetWindowData = findBestNeighbor(WDATA, NEXT);
                if (pTargetWindowData) {
                    focusTargetUpdate(pTargetWindowData->target.lock());
                    centerOrFit(m_scrollingData, NEXT);
                    m_scrollingData->recalculate();
                    auto w = pTargetWindowData->target.lock() ? pTargetWindowData->target.lock()->window() : nullptr;
                    if (w)
                        g_pCompositor->warpCursorTo(w->middle());
                }
                break;
            }

            default: return {};
        }
    } else if (ARGS[0] == "promote") {
        auto focusedWindow = Desktop::focusState()->window();
        if (!focusedWindow)
            return {};

        SP<SScrollingWindowData> WDATA = nullptr;
        auto                     parent = m_parent.lock();
        if (parent && parent->space()) {
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (t && t->window() == focusedWindow) {
                    WDATA = dataFor(t);
                    break;
                }
            }
        }

        if (!WDATA)
            return {};

        auto COL = WDATA->column.lock();
        auto idx = m_scrollingData->idx(COL);
        auto col = idx == -1 ? m_scrollingData->add() : m_scrollingData->add(idx);

        COL->remove(WDATA->target.lock());

        // Remove empty column
        if (COL->windowDatas.empty())
            m_scrollingData->remove(COL);

        col->add(WDATA);

        m_scrollingData->recalculate();
    } else if (ARGS[0] == "swapcol") {
        if (ARGS.size() < 2)
            return {};

        auto focusedWindow = Desktop::focusState()->window();
        if (!focusedWindow)
            return {};

        SP<SScrollingWindowData> WDATA = nullptr;
        auto                     parent = m_parent.lock();
        if (parent && parent->space()) {
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (t && t->window() == focusedWindow) {
                    WDATA = dataFor(t);
                    break;
                }
            }
        }

        if (!WDATA)
            return {};

        const auto CURRENT_COL = WDATA->column.lock();
        if (!CURRENT_COL || m_scrollingData->columns.size() < 2)
            return {};

        const int64_t current_idx = m_scrollingData->idx(CURRENT_COL);
        const size_t  col_count   = m_scrollingData->columns.size();

        if (current_idx == -1)
            return {};

        const std::string& direction  = ARGS[1];
        int64_t            target_idx = -1;

        if (direction == "l")
            target_idx = (current_idx == 0) ? (col_count - 1) : (current_idx - 1);
        else if (direction == "r")
            target_idx = (current_idx == (int64_t)col_count - 1) ? 0 : (current_idx + 1);
        else
            return {};

        std::swap(m_scrollingData->columns[current_idx], m_scrollingData->columns[target_idx]);
        m_scrollingData->centerOrFitCol(CURRENT_COL);
        m_scrollingData->recalculate();
    } else if (ARGS[0] == "togglefit") {
        static const auto PFITMETHOD = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:focus_fit_method");
        auto&             fitMethod  = *PFITMETHOD.ptr();
        const int         toggled    = fitMethod ^ 1;

        fitMethod = toggled;

        auto focusedWindow = Desktop::focusState()->window();

        SP<SScrollingWindowData> focusedData = nullptr;
        auto                     parent = m_parent.lock();
        if (parent && parent->space() && focusedWindow) {
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (t && t->window() == focusedWindow) {
                    focusedData = dataFor(t);
                    break;
                }
            }
        }

        static const auto PFSONONE = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:fullscreen_on_one_column");

        if (m_scrollingData->columns.empty())
            return {};

        const auto USABLE = usableArea();

        const auto focusedColumn = (focusedData && focusedData->column.lock()) ? focusedData->column.lock() : nullptr;
        const auto fallbackColumn = m_scrollingData->atCenter();

        if (toggled == 1) {
            const auto columnToFit = focusedColumn ? focusedColumn : fallbackColumn;
            if (!columnToFit)
                return {};

            double currentLeft = 0.0;
            for (const auto& col : m_scrollingData->columns) {
                const double itemWidth = *PFSONONE && m_scrollingData->columns.size() == 1 ? USABLE.w : USABLE.w * col->columnWidth;

                if (col == columnToFit) {
                    const double colRight  = currentLeft + itemWidth;
                    const double scrollMax = std::max(m_scrollingData->maxWidth() - USABLE.w, 0.0);
                    double       desiredOffset;

                    if (col == m_scrollingData->columns.front())
                        desiredOffset = 0.0;
                    else
                        desiredOffset = std::clamp(colRight - USABLE.w, 0.0, scrollMax);

                    m_scrollingData->leftOffset = desiredOffset;
                    break;
                }

                currentLeft += itemWidth;
            }
        } else {
            const auto columnToCenter = focusedColumn ? focusedColumn : fallbackColumn;
            if (!columnToCenter)
                return {};

            m_scrollingData->centerCol(columnToCenter);
        }

        m_scrollingData->recalculate();
    } else if (ARGS[0] == "pin") {
        // pin left / pin right — pin the focused column to a screen edge
        auto focusedWindow = Desktop::focusState()->window();
        if (!focusedWindow)
            return {};

        SP<SScrollingWindowData> WDATA = nullptr;
        auto                     parent = m_parent.lock();
        if (parent && parent->space()) {
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (t && t->window() == focusedWindow) {
                    WDATA = dataFor(t);
                    break;
                }
            }
        }
        if (!WDATA)
            return {};

        auto COL = WDATA->column.lock();
        if (!COL)
            return {};

        if (ARGS.size() < 2 || ARGS[1].empty())
            return {};

        if (ARGS[1] == "left")
            COL->pinned = PIN_LEFT;
        else if (ARGS[1] == "right")
            COL->pinned = PIN_RIGHT;
        else
            return {};

        m_scrollingData->recalculate();
    } else if (ARGS[0] == "unpin") {
        // unpin — unpin the focused column
        auto focusedWindow = Desktop::focusState()->window();
        if (!focusedWindow)
            return {};

        SP<SScrollingWindowData> WDATA = nullptr;
        auto                     parent = m_parent.lock();
        if (parent && parent->space()) {
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (t && t->window() == focusedWindow) {
                    WDATA = dataFor(t);
                    break;
                }
            }
        }
        if (!WDATA)
            return {};

        auto COL = WDATA->column.lock();
        if (!COL)
            return {};

        COL->pinned = PIN_NONE;
        m_scrollingData->recalculate();
    } else if (ARGS[0] == "movecoltoworkspace") {
        // movecoltoworkspace <workspace> — move all windows in current column to another workspace
        if (ARGS.size() < 2 || ARGS[1].empty())
            return {};

        auto focusedWindow = Desktop::focusState()->window();
        if (!focusedWindow)
            return {};

        SP<SScrollingWindowData> WDATA = nullptr;
        auto                     parent = m_parent.lock();
        if (parent && parent->space()) {
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (t && t->window() == focusedWindow) {
                    WDATA = dataFor(t);
                    break;
                }
            }
        }
        if (!WDATA)
            return {};

        auto COL = WDATA->column.lock();
        if (!COL)
            return {};

        moveColToWorkspace(COL, ARGS[1]);
    } else if (ARGS[0] == "collapse") {
        auto focusedWindow = Desktop::focusState()->window();
        if (!focusedWindow)
            return {};

        SP<SScrollingWindowData> WDATA = nullptr;
        auto                     parent = m_parent.lock();
        if (parent && parent->space()) {
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (t && t->window() == focusedWindow) {
                    WDATA = dataFor(t);
                    break;
                }
            }
        }
        if (!WDATA)
            return {};

        auto COL = WDATA->column.lock();
        if (!COL || COL->collapsed)
            return {};

        COL->savedColumnWidth = COL->columnWidth;
        COL->collapsed = true;
        m_scrollingData->recalculate();
    } else if (ARGS[0] == "expand") {
        auto focusedWindow = Desktop::focusState()->window();
        if (!focusedWindow)
            return {};

        SP<SScrollingWindowData> WDATA = nullptr;
        auto                     parent = m_parent.lock();
        if (parent && parent->space()) {
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (t && t->window() == focusedWindow) {
                    WDATA = dataFor(t);
                    break;
                }
            }
        }
        if (!WDATA)
            return {};

        auto COL = WDATA->column.lock();
        if (!COL || !COL->collapsed)
            return {};

        COL->collapsed = false;
        if (COL->savedColumnWidth > 0)
            COL->columnWidth = COL->savedColumnWidth;
        COL->savedColumnWidth = 0;
        m_scrollingData->centerOrFitCol(COL);
        m_scrollingData->recalculate();
    } else if (ARGS[0] == "togglecollapse") {
        auto focusedWindow = Desktop::focusState()->window();
        if (!focusedWindow)
            return {};

        SP<SScrollingWindowData> WDATA = nullptr;
        auto                     parent = m_parent.lock();
        if (parent && parent->space()) {
            for (auto& wt : parent->space()->targets()) {
                auto t = wt.lock();
                if (t && t->window() == focusedWindow) {
                    WDATA = dataFor(t);
                    break;
                }
            }
        }
        if (!WDATA)
            return {};

        auto COL = WDATA->column.lock();
        if (!COL)
            return {};

        if (COL->collapsed) {
            COL->collapsed = false;
            if (COL->savedColumnWidth > 0)
                COL->columnWidth = COL->savedColumnWidth;
            COL->savedColumnWidth = 0;
            m_scrollingData->centerOrFitCol(COL);
        } else {
            COL->savedColumnWidth = COL->columnWidth;
            COL->collapsed = true;
        }
        m_scrollingData->recalculate();
    } else if (ARGS[0] == "zen") {
        if (m_zenMode) {
            // Exit zen mode
            m_zenMode = false;
            m_zenColumn = nullptr;
            m_scrollingData->recalculate();
        } else {
            // Enter zen mode with focused column
            auto focusedWindow = Desktop::focusState()->window();
            if (!focusedWindow)
                return {};

            SP<SScrollingWindowData> WDATA = nullptr;
            auto                     parent = m_parent.lock();
            if (parent && parent->space()) {
                for (auto& wt : parent->space()->targets()) {
                    auto t = wt.lock();
                    if (t && t->window() == focusedWindow) {
                        WDATA = dataFor(t);
                        break;
                    }
                }
            }
            if (!WDATA)
                return {};

            auto COL = WDATA->column.lock();
            if (!COL)
                return {};

            m_zenMode = true;
            m_zenColumn = COL;
            m_scrollingData->recalculate();
        }
    } else if (ARGS[0] == "focusback") {
        static const auto PFOCUSHIST = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:focus_history");
        if (!*PFOCUSHIST || m_focusHistory.empty())
            return {};

        if (m_focusHistoryIdx < 0)
            m_focusHistoryIdx = (int64_t)m_focusHistory.size() - 1;

        // Move backward in history
        if (m_focusHistoryIdx > 0) {
            m_focusHistoryIdx--;
            m_focusHistoryNavigating = true;

            auto target = m_focusHistory[m_focusHistoryIdx].lock();
            if (target) {
                auto WDATA = dataFor(target);
                if (WDATA) {
                    m_scrollingData->centerOrFitCol(WDATA->column.lock());
                    m_scrollingData->recalculate();
                    Desktop::focusState()->fullWindowFocus(target->window(), Desktop::FOCUS_REASON_OTHER);
                    if (target->window())
                        g_pCompositor->warpCursorTo(target->window()->middle());
                }
            }

            m_focusHistoryNavigating = false;
        }
    } else if (ARGS[0] == "focusfwd") {
        static const auto PFOCUSHIST = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:focus_history");
        if (!*PFOCUSHIST || m_focusHistory.empty() || m_focusHistoryIdx < 0)
            return {};

        // Move forward in history
        if (m_focusHistoryIdx < (int64_t)m_focusHistory.size() - 1) {
            m_focusHistoryIdx++;
            m_focusHistoryNavigating = true;

            auto target = m_focusHistory[m_focusHistoryIdx].lock();
            if (target) {
                auto WDATA = dataFor(target);
                if (WDATA) {
                    m_scrollingData->centerOrFitCol(WDATA->column.lock());
                    m_scrollingData->recalculate();
                    Desktop::focusState()->fullWindowFocus(target->window(), Desktop::FOCUS_REASON_OTHER);
                    if (target->window())
                        g_pCompositor->warpCursorTo(target->window()->middle());
                }
            }

            m_focusHistoryNavigating = false;
        }
    }
    return {};
}

std::optional<Vector2D> CScrollingLayout::predictSizeForNewTarget() {
    return std::nullopt;
}

void CScrollingLayout::swapTargets(SP<Layout::ITarget> a, SP<Layout::ITarget> b) {
    auto DATA1 = dataFor(a);
    auto DATA2 = dataFor(b);

    if (!DATA1 || !DATA2)
        return;

    std::swap(DATA1->target, DATA2->target);

    m_scrollingData->recalculate();
}

void CScrollingLayout::moveTargetInDirection(SP<Layout::ITarget> t, Math::eDirection dir, bool silent) {
    moveTargetTo(t, dir, silent);
}

void CScrollingLayout::moveTargetTo(SP<Layout::ITarget> t, Math::eDirection dir, bool silent) {
    const auto DATA = dataFor(t);

    if (!DATA)
        return;

    auto COL = DATA->column.lock();
    if (!COL)
        return;

    if (dir == Math::DIRECTION_LEFT) {
        const auto PREVCOL = m_scrollingData->prev(COL);

        COL->remove(t);
        if (COL->windowDatas.empty())
            m_scrollingData->remove(COL);

        if (!PREVCOL) {
            const auto NEWCOL = m_scrollingData->add(-1);
            NEWCOL->add(DATA);
            m_scrollingData->centerOrFitCol(NEWCOL);
        } else {
            if (PREVCOL->windowDatas.size() > 0)
                PREVCOL->add(DATA, PREVCOL->idxForHeight(g_pInputManager->getMouseCoordsInternal().y));
            else
                PREVCOL->add(DATA);
            m_scrollingData->centerOrFitCol(PREVCOL);
        }
    } else if (dir == Math::DIRECTION_RIGHT) {
        const auto NEXTCOL = m_scrollingData->next(COL);

        COL->remove(t);
        if (COL->windowDatas.empty())
            m_scrollingData->remove(COL);

        if (!NEXTCOL) {
            const auto NEWCOL = m_scrollingData->add();
            NEWCOL->add(DATA);
            m_scrollingData->centerOrFitCol(NEWCOL);
        } else {
            if (NEXTCOL->windowDatas.size() > 0)
                NEXTCOL->add(DATA, NEXTCOL->idxForHeight(g_pInputManager->getMouseCoordsInternal().y));
            else
                NEXTCOL->add(DATA);
            m_scrollingData->centerOrFitCol(NEXTCOL);
        }

    } else if (dir == Math::DIRECTION_UP)
        COL->up(DATA);
    else if (dir == Math::DIRECTION_DOWN)
        COL->down(DATA);

    m_scrollingData->recalculate();
    focusTargetUpdate(t);
    auto w = t->window();
    if (w)
        g_pCompositor->warpCursorTo(w->middle());
}

SP<SScrollingWindowData> CScrollingLayout::dataFor(SP<Layout::ITarget> t) {
    if (!t)
        return nullptr;

    for (const auto& c : m_scrollingData->columns) {
        for (const auto& d : c->windowDatas) {
            if (d->target.lock() == t)
                return d;
        }
    }

    return nullptr;
}

void CScrollingLayout::moveColToWorkspace(SP<SColumnData> col, const std::string& wsStr) {
    if (!col || col->windowDatas.empty())
        return;

    // Resolve workspace
    const auto WSIDNAME = getWorkspaceIDNameFromString(wsStr);
    if (WSIDNAME.id == WORKSPACE_INVALID)
        return;

    auto parent = m_parent.lock();
    if (!parent || !parent->space())
        return;

    auto CURRENTWS = parent->space()->workspace();
    if (!CURRENTWS)
        return;

    // Get or create target workspace
    auto TARGETWS = g_pCompositor->getWorkspaceByID(WSIDNAME.id);
    if (!TARGETWS) {
        TARGETWS = g_pCompositor->createNewWorkspace(WSIDNAME.id, CURRENTWS->m_monitor.lock() ? CURRENTWS->m_monitor.lock()->m_id : 0, WSIDNAME.name);
    }

    if (!TARGETWS || TARGETWS == CURRENTWS)
        return;

    // Collect all windows from the column
    std::vector<PHLWINDOW> windowsToMove;
    for (const auto& wd : col->windowDatas) {
        auto target = wd->target.lock();
        if (target && target->window())
            windowsToMove.push_back(target->window());
    }

    // Move each window to the target workspace
    for (const auto& w : windowsToMove) {
        g_pCompositor->moveWindowToWorkspaceSafe(w, TARGETWS);
    }
}

void CScrollingLayout::pushFocusHistory(SP<Layout::ITarget> target) {
    static const auto PFOCUSHIST = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:focus_history");
    if (!*PFOCUSHIST || !target || m_focusHistoryNavigating)
        return;

    // If we're not at the end of history, truncate forward history
    if (m_focusHistoryIdx >= 0 && m_focusHistoryIdx < (int64_t)m_focusHistory.size() - 1) {
        m_focusHistory.erase(m_focusHistory.begin() + m_focusHistoryIdx + 1, m_focusHistory.end());
    }

    // Don't push duplicates
    if (!m_focusHistory.empty()) {
        auto last = m_focusHistory.back().lock();
        if (last == target)
            return;
    }

    m_focusHistory.push_back(target);

    // Limit history size
    constexpr size_t MAX_HISTORY = 50;
    while (m_focusHistory.size() > MAX_HISTORY) {
        m_focusHistory.pop_front();
    }

    m_focusHistoryIdx = (int64_t)m_focusHistory.size() - 1;
}

float CScrollingLayout::autoWidthForTarget(SP<Layout::ITarget> target) {
    if (!target || m_config.autoWidthRules.empty())
        return defaultColumnWidth();

    auto w = target->window();
    if (!w)
        return defaultColumnWidth();

    // Check window class
    const std::string cls = w->m_class;
    auto it = m_config.autoWidthRules.find(cls);
    if (it != m_config.autoWidthRules.end())
        return it->second;

    return defaultColumnWidth();
}
