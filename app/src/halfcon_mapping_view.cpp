#ifdef __SWITCH__
#include "halfcon_mapping_view.hpp"

#include <algorithm>
#include <array>
#include <memory>

#include "Settings.hpp"

using namespace brls;
using namespace brls::literals;

namespace
{
const std::array<int, 18> mappingActions = { HALF_JOYCON_UNMAPPED, BUTTON_B, BUTTON_A, BUTTON_Y, BUTTON_X, BUTTON_BACK,
    BUTTON_START, BUTTON_GUIDE, BUTTON_LB, BUTTON_RB, BUTTON_LT, BUTTON_RT, BUTTON_LSB, BUTTON_RSB, BUTTON_UP,
    BUTTON_RIGHT, BUTTON_DOWN, BUTTON_LEFT };

std::vector<std::string> actionNames()
{
    const auto icon = [](ControllerButton button) { return Hint::getKeyIcon(button, true); };
    return { "halfcon/unmapped"_i18n, icon(BUTTON_A), icon(BUTTON_B), icon(BUTTON_X), icon(BUTTON_Y),
        icon(BUTTON_BACK) + "  " + "halfcon/select"_i18n, icon(BUTTON_START) + "  " + "halfcon/start"_i18n,
        "\uE0F4  " + "halfcon/guide"_i18n, icon(BUTTON_LB), icon(BUTTON_RB), icon(BUTTON_LT), icon(BUTTON_RT),
        icon(BUTTON_LSB), icon(BUTTON_RSB), icon(BUTTON_UP), icon(BUTTON_RIGHT), icon(BUTTON_DOWN), icon(BUTTON_LEFT) };
}

std::string sourceIcon(HalfJoyConSide side, size_t index)
{
    const bool left                         = side == HalfJoyConSide::LEFT;
    const auto icon                         = [](ControllerButton button) { return Hint::getKeyIcon(button, true); };
    const std::array<std::string, 12> icons = { icon(left ? BUTTON_UP : BUTTON_Y), icon(left ? BUTTON_RIGHT : BUTTON_X),
        icon(left ? BUTTON_DOWN : BUTTON_A), icon(left ? BUTTON_LEFT : BUTTON_B), icon(left ? BUTTON_LSB : BUTTON_RSB),
        "\uE0E8", "\uE0E9", left ? "\uE0F2" : "\uE0F1", left ? "\uE0F5" : "\uE0F4", icon(left ? BUTTON_LB : BUTTON_RB),
        icon(left ? BUTTON_LT : BUTTON_RT), "\uE04A" };
    return icons[index];
}

class JoyConDiagram : public View
{
  public:
    HalfJoyConSide side;
    const bool secondGeneration;
    int selected = -1;
    JoyConDiagram(HalfJoyConSide side, bool second)
        : side(side)
        , secondGeneration(second)
    {
        setHeight(190);
        setShrink(0);
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, Style, FrameContext*) override
    {
        nvgSave(vg);
        const float scale = std::min(width / 460.0f, height / 190.0f);
        nvgTranslate(vg, x + (width - 460 * scale) / 2, y);
        nvgScale(vg, scale, scale);
        const bool left       = side == HalfJoyConSide::LEFT;
        const NVGcolor accent = left ? nvgRGB(56, 189, 234) : nvgRGB(244, 97, 107);
        const NVGcolor body   = secondGeneration ? nvgRGB(49, 52, 57) : accent;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 20, 43, 420, 110, 48);
        nvgFillColor(vg, body);
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 98, 34, 264, 12, 5);
        nvgFillColor(vg, secondGeneration ? accent : nvgRGB(68, 72, 78));
        nvgFill(vg);
        nvgFontFaceId(vg, Application::getFont(FONT_SWITCH_ICONS));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        const auto button = [&](int index, float bx, float by, float radius)
        {
            if (selected == index)
            {
                nvgBeginPath(vg);
                nvgCircle(vg, bx, by, radius + 3);
                nvgStrokeColor(vg, Application::getTheme()["brls/accent"]);
                nvgStrokeWidth(vg, 3);
                nvgStroke(vg);
            }
            nvgFillColor(vg, nvgRGB(255, 255, 255));
            nvgFontSize(vg, radius * 2);
            const auto glyph = sourceIcon(side, index);
            nvgText(vg, bx, by, glyph.c_str(), nullptr);
        };
        const float stickX = left ? 125 : 225;
        const float faceX  = left ? 245 : 335;
        button(4, stickX, 104, 28);
        button(0, faceX, 77, 15);
        button(1, faceX + 27, 104, 15);
        button(2, faceX, 131, 15);
        button(3, faceX - 27, 104, 15);
        button(5, 165, 36, 17);
        button(6, 265, 36, 17);
        button(7, left ? 52 : 404, 62, 14);
        button(8, left ? 345 : 145, 73, 15);
        if (!left && secondGeneration)
            button(11, 88, 73, 15);
        button(9, left ? 24 : 436, 72, 16);
        button(10, left ? 24 : 436, 123, 16);
        if (selected >= 0)
        {
            const auto& mapping = Settings::instance().get_halfcon_mapping(side);
            const auto position = std::find(mappingActions.begin(), mappingActions.end(), mapping[selected]);
            const auto name = actionNames()[position == mappingActions.end() ? 0 : position - mappingActions.begin()];
            const std::string text = sourceIcon(side, selected) + "  →  " + name;
            nvgFontFaceId(vg, Application::getFont(FONT_REGULAR));
            nvgFontSize(vg, 18);
            nvgFillColor(vg, Application::getTheme()["brls/text"]);
            nvgText(vg, 230, 177, text.c_str(), nullptr);
        }
        nvgRestore(vg);
    }
};
} // namespace

HalfConMappingView::HalfConMappingView()
    : Box(Axis::COLUMN)
{
    getAppletFrameItem()->title = "halfcon/title"_i18n;
    setGrow(1);
    setPaddingLeft(24);
    setPaddingRight(24);
#ifdef __SWITCH2__
    constexpr bool second = true;
#else
    constexpr bool second = false;
#endif
    auto* columns = new Box(Axis::ROW);
    columns->setGrow(1);
    addView(columns);
    for (int index = 0; index < 2; ++index)
    {
        const auto side = index == 0 ? HalfJoyConSide::LEFT : HalfJoyConSide::RIGHT;
        auto* column    = new Box(Axis::COLUMN);
        column->setWidthPercentage(50);
        column->setPaddingRight(index == 0 ? 10 : 0);
        column->setPaddingLeft(index == 1 ? 10 : 0);
        columns->addView(column);
        auto* title = new Header();
        title->setTitle(index == 0 ? "halfcon/left"_i18n : "halfcon/right"_i18n);
        column->addView(title);
        auto* diagram = new JoyConDiagram(side, second);
        column->addView(diagram);
        auto* scroll = new ScrollingFrame();
        scroll->setHeight(0);
        scroll->setGrow(1);
        column->addView(scroll);
        auto* cells = new Box(Axis::COLUMN);
        scroll->setContentView(cells);
        auto* reset = new DetailCell();
        reset->setText("halfcon/reset"_i18n);
        cells->addView(reset);
        auto bindings      = std::make_shared<std::vector<SelectorCell*>>();
        const size_t count = side == HalfJoyConSide::LEFT || !second ? 11 : 12;
        for (size_t key = 0; key < count; ++key)
        {
            const auto& mapping = Settings::instance().get_halfcon_mapping(side);
            const auto found    = std::find(mappingActions.begin(), mappingActions.end(), mapping[key]);
            auto* cell          = new SelectorCell();
            cell->init(sourceIcon(side, key), actionNames(),
                found == mappingActions.end() ? 0 : found - mappingActions.begin(),
                [side, key](int selected)
                {
                    auto mapping = Settings::instance().get_halfcon_mapping(side);
                    mapping[key] = mappingActions.at(selected);
                    Settings::instance().set_halfcon_mapping(side, mapping);
                });
            cell->getFocusEvent()->subscribe([diagram, key](View*) { diagram->selected = static_cast<int>(key); });
            cells->addView(cell);
            bindings->push_back(cell);
        }
        reset->registerClickAction(
            [side, bindings](View*)
            {
                const auto mapping = defaultHalfJoyConMapping(side);
                Settings::instance().set_halfcon_mapping(side, mapping);
                for (size_t i = 0; i < bindings->size(); ++i)
                    bindings->at(i)->setSelection(
                        std::find(mappingActions.begin(), mappingActions.end(), mapping[i]) - mappingActions.begin(),
                        true);
                return true;
            });
    }
}

HalfConMappingView::~HalfConMappingView() { Settings::instance().save(); }
#endif
