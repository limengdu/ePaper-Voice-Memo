#include "TextRenderer.h"

#include "UiLang.h"

#if VM_LANG_ZH

#include <SPIFFS.h>

#include "OpenFontRender.h"
#include "OfrSpiffs.h"   // OFR_f* hooks + globals; this TU only.

namespace {

// Single renderer bound to the panel (Chinese build only).
OpenFontRender g_ofr;

// Maps the bitmap "size unit" (the old setTextSize scale, ~8 px per unit) to
// OpenFontRender pixels. Tune on hardware so Chinese glyphs visually match the
// former bitmap sizes.
constexpr int VM_ZH_PX_PER_UNIT = 8;

}  // namespace

#else  // English build: map alignment onto TFT_eSPI text datums.

namespace {

uint8_t toTftDatum(TextAlign a) {
  switch (a) {
    case TextAlign::TopLeft:      return TL_DATUM;
    case TextAlign::TopCenter:    return TC_DATUM;
    case TextAlign::TopRight:     return TR_DATUM;
    case TextAlign::MiddleLeft:   return ML_DATUM;
    case TextAlign::MiddleCenter: return MC_DATUM;
    case TextAlign::MiddleRight:  return MR_DATUM;
    case TextAlign::BottomLeft:   return BL_DATUM;
    case TextAlign::BottomCenter: return BC_DATUM;
    case TextAlign::BottomRight:  return BR_DATUM;
  }
  return TL_DATUM;
}

}  // namespace

#endif

bool TextRenderer::begin(EPaper& display)
{
  display_ = &display;
#if VM_LANG_ZH
  if (!SPIFFS.begin()) {
    fontReady_ = false;
    Serial1.println("[ofr] SPIFFS mount failed");
    return false;
  }
  g_ofr.setDrawer(static_cast<TFT_eSPI&>(display));
  // loadFont returns non-zero on failure.
  if (g_ofr.loadFont("/test_ZH.ttf")) {
    fontReady_ = false;
    Serial1.println("[ofr] loadFont /test_ZH.ttf failed");
    return false;
  }
  g_ofr.showCredit();   // FreeType FTL license attribution
  fontReady_ = true;
  return true;
#else
  fontReady_ = true;    // bitmap font is always available
  return true;
#endif
}

void TextRenderer::drawText(const String& text, int x, int y, int sizeUnit,
                            TextAlign align, uint16_t color, uint16_t bg)
{
  if (!display_) return;
#if VM_LANG_ZH
  // Never call into OpenFontRender without a loaded font: it dereferences a
  // null face and crashes. Degrade by skipping the glyphs instead.
  if (!fontReady_) return;

  const unsigned px = static_cast<unsigned>(sizeUnit * VM_ZH_PX_PER_UNIT);
  g_ofr.setFontSize(px);
  // "%s" wrapper: getTextWidth is printf-style, so a literal '%' in the text
  // would otherwise be read as a format specifier.
  const int w = static_cast<int>(g_ofr.getTextWidth("%s", text.c_str()));
  const int h = static_cast<int>(px);

  // Resolve the anchor to a top-left origin ourselves, then draw with
  // Align::TopLeft. OFR's other alignment modes are reported as fiddly, and
  // TopLeft is the mode whose behavior is least ambiguous.
  int ox = x;
  int oy = y;
  switch (align) {
    case TextAlign::TopCenter:
    case TextAlign::MiddleCenter:
    case TextAlign::BottomCenter: ox = x - w / 2; break;
    case TextAlign::TopRight:
    case TextAlign::MiddleRight:
    case TextAlign::BottomRight:  ox = x - w; break;
    default: break;
  }
  switch (align) {
    case TextAlign::MiddleLeft:
    case TextAlign::MiddleCenter:
    case TextAlign::MiddleRight:  oy = y - h / 2; break;
    case TextAlign::BottomLeft:
    case TextAlign::BottomCenter:
    case TextAlign::BottomRight:  oy = y - h; break;
    default: break;
  }

  FT_BBox bbox;
  FT_Error error;
  g_ofr.drawHString(text.c_str(), ox, oy, color, bg,
                    Align::TopLeft, Drawing::Execute, bbox, error);
#else
  display_->setTextSize(sizeUnit);
  display_->setTextColor(color, bg, true);
  display_->setTextDatum(toTftDatum(align));
  display_->drawString(text, x, y);
#endif
}

int TextRenderer::measureText(const String& text, int sizeUnit)
{
  if (!display_) return 0;
#if VM_LANG_ZH
  if (!fontReady_) return 0;
  g_ofr.setFontSize(static_cast<unsigned>(sizeUnit * VM_ZH_PX_PER_UNIT));
  return static_cast<int>(g_ofr.getTextWidth("%s", text.c_str()));
#else
  display_->setTextSize(sizeUnit);
  return static_cast<int>(display_->textWidth(text));
#endif
}
