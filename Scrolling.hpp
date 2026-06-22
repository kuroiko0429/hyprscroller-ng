#pragma once

#include <vector>
#include <deque>
#include <unordered_map>
#include <string>
#include <hyprland/src/layout/algorithm/TiledAlgorithm.hpp>
#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/helpers/signal/Signal.hpp>
#include <hyprland/src/config/shared/ConfigErrors.hpp>

class CScrollingLayout;
struct SColumnData;

enum ePinState : uint8_t {
    PIN_NONE = 0,
    PIN_LEFT,
    PIN_RIGHT
};

struct SScrollingWindowData {
    SScrollingWindowData(SP<Layout::ITarget> t, SP<SColumnData> col, float ws = 1.F) : target(t), column(col), windowSize(ws) {
        ;
    }

    WP<Layout::ITarget> target;
    WP<SColumnData>     column;
    float               windowSize             = 1.F;
    bool                ignoreFullscreenChecks = false;

    CBox                layoutBox;
};

struct SColumnData {
    SColumnData(CScrollingLayout* layout) : layout(layout) {
        ;
    }

    void                                  add(SP<Layout::ITarget> t);
    void                                  add(SP<Layout::ITarget> t, int after);
    void                                  add(SP<SScrollingWindowData> w);
    void                                  add(SP<SScrollingWindowData> w, int after);
    void                                  remove(SP<Layout::ITarget> t);
    bool                                  has(SP<Layout::ITarget> t);

    // index of lowest window that is above y.
    size_t                                idxForHeight(float y);

    void                                  up(SP<SScrollingWindowData> w);
    void                                  down(SP<SScrollingWindowData> w);

    SP<SScrollingWindowData>              next(SP<SScrollingWindowData> w);
    SP<SScrollingWindowData>              prev(SP<SScrollingWindowData> w);

    std::vector<SP<SScrollingWindowData>> windowDatas;
    float                                 columnSize       = 1.F;
    float                                 columnWidth      = 1.F;
    float                                 savedColumnWidth = 0.F;
    bool                                  collapsed        = false;
    ePinState                             pinned           = PIN_NONE;
    CScrollingLayout*                     layout           = nullptr;
    WP<SScrollingWindowData>              lastFocusedWindow;

    WP<SColumnData>                       self;
};

struct SScrollingLayoutData {
    SScrollingLayoutData(CScrollingLayout* l) : layout(l) {
        ;
    }

    std::vector<SP<SColumnData>> columns;
    float                        leftOffset = 0;

    SP<SColumnData>              add();
    SP<SColumnData>              add(int after);
    int64_t                      idx(SP<SColumnData> c);
    void                         remove(SP<SColumnData> c);
    double                       maxWidth();
    double                       pinnedWidthLeft();
    double                       pinnedWidthRight();
    SP<SColumnData>              next(SP<SColumnData> c);
    SP<SColumnData>              prev(SP<SColumnData> c);
    SP<SColumnData>              atCenter();

    bool                         visible(SP<SColumnData> c);
    void                         centerCol(SP<SColumnData> c);
    void                         fitCol(SP<SColumnData> c);
    void                         centerOrFitCol(SP<SColumnData> c);

    void                         recalculate(bool forceInstant = false);

    CScrollingLayout*            layout = nullptr;
    WP<SScrollingLayoutData>     self;
};

class CScrollingLayout : public Layout::ITiledAlgorithm {
  public:
    CScrollingLayout();
    virtual ~CScrollingLayout();

    virtual void                                          newTarget(SP<Layout::ITarget> target);
    virtual void                                          movedTarget(SP<Layout::ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt);
    virtual void                                          removeTarget(SP<Layout::ITarget> target);

    virtual void                                          resizeTarget(const Vector2D& delta, SP<Layout::ITarget> target, Layout::eRectCorner corner = Layout::CORNER_NONE);
    virtual void                                          recalculate(Layout::eRecalculateReason reason = Layout::RECALCULATE_REASON_UNKNOWN);

    virtual SP<Layout::ITarget>                           getNextCandidate(SP<Layout::ITarget> old);

    virtual Config::ErrorResult                           layoutMsg(const std::string_view& sv);
    virtual std::optional<Vector2D>                       predictSizeForNewTarget();

    virtual void                                          swapTargets(SP<Layout::ITarget> a, SP<Layout::ITarget> b);
    virtual void                                          moveTargetInDirection(SP<Layout::ITarget> t, Math::eDirection dir, bool silent);

    CBox                                                  usableArea();

  private:
    SP<SScrollingLayoutData> m_scrollingData;

    CHyprSignalListener      m_configCallback;
    CHyprSignalListener      m_focusCallback;

    struct {
        std::vector<float>                           configuredWidths;
        std::unordered_map<std::string, float>        autoWidthRules;
    } m_config;

    // Focus history
    std::deque<WP<Layout::ITarget>>                  m_focusHistory;
    int64_t                                          m_focusHistoryIdx = -1;
    bool                                             m_focusHistoryNavigating = false;

    // Zen mode
    bool                                             m_zenMode = false;
    SP<SColumnData>                                  m_zenColumn;

    SP<SScrollingWindowData> findBestNeighbor(SP<SScrollingWindowData> pCurrent, SP<SColumnData> pTargetCol);
    SP<SScrollingWindowData> dataFor(SP<Layout::ITarget> t);

    void                     focusTargetUpdate(SP<Layout::ITarget> target);
    void                     pushFocusHistory(SP<Layout::ITarget> target);
    void                     moveTargetTo(SP<Layout::ITarget> t, Math::eDirection dir, bool silent);
    void                     moveColToWorkspace(SP<SColumnData> col, const std::string& wsStr);
    float                    autoWidthForTarget(SP<Layout::ITarget> target);

    float                    defaultColumnWidth();

    friend struct SScrollingLayoutData;
};
