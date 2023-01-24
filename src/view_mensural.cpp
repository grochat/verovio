/////////////////////////////////////////////////////////////////////////////
// Name:        view_mensural.cpp
// Author:      Laurent Pugin
// Created:     2015
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "view.h"

//----------------------------------------------------------------------------

#include <assert.h>

//----------------------------------------------------------------------------

#include "devicecontext.h"
#include "doc.h"
#include "dot.h"
#include "elementpart.h"
#include "layer.h"
#include "ligature.h"
#include "mensur.h"
#include "note.h"
#include "options.h"
#include "plica.h"
#include "proport.h"
#include "rest.h"
#include "smufl.h"
#include "staff.h"
#include "vrv.h"

namespace vrv {

int View::s_drawingLigX[2], View::s_drawingLigY[2]; // to keep coords. of ligatures
bool View::s_drawingLigObliqua = false; // mark the first pass for an oblique

//----------------------------------------------------------------------------
// View - Mensural
//----------------------------------------------------------------------------

void View::DrawMensuralNote(DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff, Measure *measure)
{
    assert(dc);
    assert(element);
    assert(layer);
    assert(staff);
    assert(measure);

    Note *note = vrv_cast<Note *>(element);
    assert(note);

    const int yNote = element->GetDrawingY();
    const int xNote = element->GetDrawingX();
    const int drawingDur = note->GetDrawingDur();
    const int radius = note->GetDrawingRadius(m_doc);
    const int staffY = staff->GetDrawingY();
    const bool mensural_black = (staff->m_drawingNotationType == NOTATIONTYPE_mensural_black);

    /************** Stem/notehead direction: **************/

    data_STEMDIRECTION layerStemDir;
    data_STEMDIRECTION stemDir = STEMDIRECTION_NONE;

    int verticalCenter = staffY - m_doc->GetDrawingDoubleUnit(staff->m_drawingStaffSize) * 2;
    if (note->HasStemDir()) {
        stemDir = note->GetStemDir();
    }
    else if ((layerStemDir = layer->GetDrawingStemDir(note)) != STEMDIRECTION_NONE) {
        stemDir = layerStemDir;
    }
    else {
        if (drawingDur < DUR_1) {
            stemDir = STEMDIRECTION_down;
        }
        else {
            stemDir = (yNote > verticalCenter) ? STEMDIRECTION_down : STEMDIRECTION_up;
        }
    }

    if (note->GetHeadVisible() == BOOLEAN_false)
    {
        dc->StartGraphic(element, "", element->GetUuid());
        dc->DrawPlaceholder(ToDeviceContextX(element->GetDrawingX()), ToDeviceContextY(element->GetDrawingY()));
        dc->EndGraphic(element, this);
    }
    else {
        // Ligature, maxima,longa, and brevis
        if (note->IsInLigature()) {
            DrawLigatureNote(dc, element, layer, staff);
        }
        else if (drawingDur < DUR_1) {
            DrawMaximaToBrevis(dc, yNote, element, layer, staff);
        }
        // Semibrevis and shorter
        else {
            if ( m_doc->GetOptions()->m_useGlyphMensural.GetValue() )
            {
                wchar_t code = -1;
                switch (drawingDur)
                {
                    case DUR_1:
                    {
                        if ( note->HasStemDir() && stemDir == STEMDIRECTION_down)
                            code = SMUFL_E959_mensuralBlackSemibrevisCaudata;
                        else
                            code = SMUFL_E953_mensuralBlackSemibrevis;
                        break;
                    }
                        
                    case DUR_2:
                    {
                        code = SMUFL_E954_mensuralBlackMinima;
                        
                        if ( note->HasStemDir() && stemDir == STEMDIRECTION_down)
                            code = SMUFL_F703_mensuralBlackMinimaStemDown;
                        else
                            code = SMUFL_E954_mensuralBlackMinima;
                        break;
                    }
                        
                    case DUR_4:
                    {
                        code = SMUFL_E955_mensuralBlackSemiminima;
                        break;
                    }
                        
                    case DUR_8:
                    {
                        //Available in Machaut font only:
                        if ( Resources::IsGlyphAvailable((wchar_t) SMUFL_F702_mensuralBlackFusa) )
                            code = SMUFL_F702_mensuralBlackFusa;
                        break;
                    }
                }
                if ( code != -1 )
                {
                    dc->StartCustomGraphic("notehead");
                    DrawSmuflCode(dc, xNote, yNote, code, staff->m_drawingStaffSize, false);
                    dc->EndCustomGraphic();
                }
            }
            else
            {
                wchar_t code = note->GetMensuralNoteheadGlyph();
                dc->StartCustomGraphic("notehead");
                DrawSmuflCode(dc, xNote, yNote, code, staff->m_drawingStaffSize, false);
                dc->EndCustomGraphic();
                // For semibrevis with stem in black notation, encoded with an explicit stem direction
                if (((drawingDur > DUR_1) || (note->GetStemDir() != STEMDIRECTION_NONE))
                    && note->GetStemVisible() != BOOLEAN_false) {
                    DrawMensuralStem(dc, note, staff, stemDir, radius, xNote, yNote);
                }
                dc->EndCustomGraphic();
            }
        }
    }

    /************ Draw children (verse / syl) ************/

    DrawLayerChildren(dc, note, layer, staff, measure);
}

void View::DrawMensuralRest(DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff, Measure *measure)
{
    assert(dc);
    assert(element);
    assert(layer);
    assert(staff);
    assert(measure);

    wchar_t charCode;

    Rest *rest = vrv_cast<Rest *>(element);
    assert(rest);

    const bool drawingCueSize = rest->GetDrawingCueSize();
    const int drawingDur = rest->GetActualDur();
    const int x = element->GetDrawingX();
    const int y = element->GetDrawingY();

    switch (drawingDur) {
        case DUR_MX: charCode = SMUFL_E9F0_mensuralRestMaxima; break;
        case DUR_LG: charCode = SMUFL_E9F1_mensuralRestLongaPerfecta; break;
        case DUR_2BR: charCode = SMUFL_E9F2_mensuralRestLongaImperfecta; break;
        case DUR_BR: charCode = SMUFL_E9F3_mensuralRestBrevis; break;
        case DUR_1: charCode = SMUFL_E9F4_mensuralRestSemibrevis; break;
        case DUR_2: charCode = SMUFL_E9F5_mensuralRestMinima; break;
        case DUR_4: charCode = SMUFL_E9F6_mensuralRestSemiminima; break;
        case DUR_8: charCode = SMUFL_E9F7_mensuralRestFusa; break;
        case DUR_16: charCode = SMUFL_E9F8_mensuralRestSemifusa; break;
        default: charCode = 0; // This should never happen
    }
    DrawSmuflCode(dc, x, y, charCode, staff->m_drawingStaffSize, drawingCueSize);
}

void View::DrawMensur(DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff, Measure *measure)
{
    assert(dc);
    assert(element);
    assert(layer);
    assert(staff);
    assert(measure);

    Mensur *mensur = vrv_cast<Mensur *>(element);
    assert(mensur);

    if (!mensur->HasSign()) {
        // only react to visual attributes
        return;
    }

    int y = staff->GetDrawingY() - m_doc->GetDrawingUnit(staff->m_drawingStaffSize) * (staff->m_drawingLines - 1);
    int x = element->GetDrawingX();
    int perfectRadius = m_doc->GetGlyphWidth(SMUFL_E910_mensuralProlation1, staff->m_drawingStaffSize, false) / 2;
    int code = 0;

    if (mensur->HasLoc()) {
        y = staff->GetDrawingY()
            - m_doc->GetDrawingUnit(staff->m_drawingStaffSize) * (2 * staff->m_drawingLines - 2 - mensur->GetLoc());
    }

    if (mensur->GetSign() == MENSURATIONSIGN_O) {
        code = SMUFL_E911_mensuralProlation2;
    }
    else if (mensur->GetSign() == MENSURATIONSIGN_C) {
        if (mensur->GetOrient() == ORIENTATION_reversed) {
            code = SMUFL_E916_mensuralProlation7;
            // additional offset
            // perfectRadius -= 2 * perfectRadius - m_doc->GetGlyphWidth(SMUFL_E916_mensuralProlation7,
            // staff->m_drawingStaffSize, false);
        }
        else {
            code = SMUFL_E915_mensuralProlation6;
        }
    }

    dc->StartGraphic(element, "", element->GetUuid());

    DrawSmuflCode(dc, x, y, code, staff->m_drawingStaffSize, false);

    x += perfectRadius;
    // only one slash supported
    if (mensur->HasSlash()) {
        DrawSmuflCode(dc,
            x - m_doc->GetGlyphWidth(SMUFL_E925_mensuralProlationCombiningStroke, staff->m_drawingStaffSize, false) / 2,
            y, SMUFL_E925_mensuralProlationCombiningStroke, staff->m_drawingStaffSize, false);
    }
    if (mensur->GetDot() == BOOLEAN_true) {
        DrawSmuflCode(dc,
            x - m_doc->GetGlyphWidth(SMUFL_E920_mensuralProlationCombiningDot, staff->m_drawingStaffSize, false) / 2, y,
            SMUFL_E920_mensuralProlationCombiningDot, staff->m_drawingStaffSize, false);
    }

    if (mensur->HasNum()) {
        x = element->GetDrawingX();
        if (mensur->HasSign() || mensur->HasTempus()) {
            x += m_doc->GetDrawingUnit(staff->m_drawingStaffSize)
                * 6; // step forward because we have a sign or a meter symbol
        }
        int numbase = mensur->HasNumbase() ? mensur->GetNumbase() : 0;
        DrawProportFigures(dc, x, y, mensur->GetNum(), numbase, staff);
    }

    dc->EndGraphic(element, this);
} // namespace vrv

/* This function draws any flags as well as the stem. */

void View::DrawMensuralStem(
    DeviceContext *dc, Note *note, Staff *staff, data_STEMDIRECTION dir, int radius, int xn, int originY, int heightY)
{
    assert(note);

    int staffSize = staff->m_drawingStaffSize;
    int staffY = staff->GetDrawingY();
    int baseStem, totalFlagStemHeight, flagStemHeight, nbFlags;
    int drawingDur = note->GetDrawingDur();
    // Cue size is currently disabled
    bool drawingCueSize = false;
    int verticalCenter = staffY - m_doc->GetDrawingDoubleUnit(staffSize) * 2;
    bool mensural_black = (staff->m_drawingNotationType == NOTATIONTYPE_mensural_black);

    baseStem = m_doc->GetDrawingUnit(staffSize) * STANDARD_STEMLENGTH;
    flagStemHeight = m_doc->GetDrawingDoubleUnit(staffSize);
    if (drawingCueSize) {
        baseStem = m_doc->GetCueSize(baseStem);
        flagStemHeight = m_doc->GetCueSize(flagStemHeight);
    }

    nbFlags = (mensural_black ? drawingDur - DUR_2 : drawingDur - DUR_4);
    totalFlagStemHeight = flagStemHeight * (nbFlags * 2 - 1) / 2;

    /* SMuFL provides combining stem-and-flag characters with one and two flags, but
        at the moment, I'm using only the one flag ones, partly out of concern for
        possible three-flag notes. */

    /* In black notation, the semiminima gets one flag; in white notation, it gets none.
        In both cases, as in CWMN, each shorter duration gets one additional flag. */

    if (dir == STEMDIRECTION_down) {
        // flip all lengths. Exception: in mensural notation, the stem will never be at
        //   left, so leave radius as is.
        baseStem = -baseStem;
        totalFlagStemHeight = -totalFlagStemHeight;
        heightY = -heightY;
    }

    // If we have flags, add them to the height.
    int y1 = originY;
    int y2 = ((nbFlags > 0) ? (y1 + baseStem + totalFlagStemHeight) : (y1 + baseStem)) + heightY;
    int x2 = xn + radius;

    if ((dir == STEMDIRECTION_up) && (y2 < verticalCenter)) {
        y2 = verticalCenter;
    }
    else if ((dir == STEMDIRECTION_down) && (y2 > verticalCenter)) {
        y2 = verticalCenter;
    }

    // shorten the stem at its connection with the note head
    // this will not work if the pseudo size is changed
    int shortening = 0.9 * m_doc->GetDrawingUnit(staffSize);

    // LogDebug("DrawMensuralStem: drawingDur=%d mensural_black=%d nbFlags=%d", drawingDur, mensural_black, nbFlags);
    int stemY1 = (dir == STEMDIRECTION_up) ? y1 + shortening : y1 - shortening;
    int stemY2 = y2;
    if (nbFlags > 0) {
        // if we have flags, shorten the stem to make sure we have a nice overlap with the flag glyph
        int shortener
            = (drawingCueSize) ? m_doc->GetCueSize(m_doc->GetDrawingUnit(staffSize)) : m_doc->GetDrawingUnit(staffSize);
        stemY2 = (dir == STEMDIRECTION_up) ? y2 - shortener : y2 + shortener;
    }

    int halfStemWidth = m_doc->GetDrawingStemWidth(staffSize) / 2;
    // draw the stems and the flags

    dc->StartCustomGraphic("stem");
    if (dir == STEMDIRECTION_up) {

        if (nbFlags > 0) {
            for (int i = 0; i < nbFlags; ++i) {
                DrawSmuflCode(dc, x2 - halfStemWidth, stemY1 - i * flagStemHeight,
                    SMUFL_E949_mensuralCombStemUpFlagSemiminima, staff->m_drawingStaffSize, drawingCueSize);
            }
        }
        else {
            DrawFilledRectangle(dc, x2 - halfStemWidth, stemY1, x2 + halfStemWidth, stemY2);
        }
    }
    else {
        if (nbFlags > 0) {
            for (int i = 0; i < nbFlags; ++i) {
                DrawSmuflCode(dc, x2 - halfStemWidth, stemY1 + i * flagStemHeight,
                    SMUFL_E94A_mensuralCombStemDownFlagSemiminima, staff->m_drawingStaffSize, drawingCueSize);
            }
        }
        else {
            DrawFilledRectangle(dc, x2 - halfStemWidth, stemY1, x2 + halfStemWidth, stemY2);
        }
    }
    dc->EndCustomGraphic();

    // Store the stem direction ?
    note->SetDrawingStemDir(dir);
}

void View::DrawMaximaToBrevis(DeviceContext *dc, int y, LayerElement *element, Layer *layer, Staff *staff)
{
    assert(dc);
    assert(element);
    assert(layer);
    assert(staff);

    Note *note = vrv_cast<Note *>(element);
    assert(note);

    bool isMensuralBlack = (staff->m_drawingNotationType == NOTATIONTYPE_mensural_black);
    bool fillNotehead = (isMensuralBlack || note->GetColored()) && !(isMensuralBlack && note->GetColored());
  
    int yNote = element->GetDrawingY();
    int xNote = element->GetDrawingX();
    wchar_t code = -1;
    int duration = note->GetActualDur();
    
    //FontInfo *font = m_doc->GetDrawingSmuflFont(staff->m_drawingStaffSize, false);
    //float r = font->GetWidthToHeightRatio();
    if ( m_doc->GetOptions()->m_useGlyphMensural.GetValue() )
    {
        if ( note->FindDescendantByType(PLICA) )    //will be drawn as a plica glyph in DrawPlica
            return;
        switch (duration)
        {
            case DUR_MX:
            {
                code = SMUFL_E950_mensuralBlackMaxima;
                break;
            }
                
            case DUR_LG:
            {
                if ( note->HasStemDir() && note->GetStemDir() == STEMDIRECTION_up )
                {
                    if ( note->HasStemPos() && note->GetStemPos() == STEMPOSITION_left )
                        code = SMUFL_F708_mensuralBlackLongaStemUpLeft;
                    else
                        code = SMUFL_F707_mensuralBlackLongaStemUpRight;
                }
                else
                    code = SMUFL_E951_mensuralBlackLonga;
                //float f = r*(1.+(float)(rand() % 100)/100.);
                //font->SetWidthToHeightRatio(f);
                break;
            }
                
            case DUR_BR:
            {
                if ( note->HasStemDir() && note->GetStemDir() == STEMDIRECTION_down
                     && note->HasStemPos() && note->GetStemPos() == STEMPOSITION_left )
                    code = SMUFL_F709_mensuralBlackBrevisStemDownLeft;
                else
                    code = SMUFL_E952_mensuralBlackBrevis;
                break;
            }
        }
    }
    
    if ( code != -1 )
    {
        dc->StartCustomGraphic("notehead");
        DrawSmuflCode(dc, xNote, yNote, code, staff->m_drawingStaffSize, false);
        //font->SetWidthToHeightRatio(r);
        dc->EndCustomGraphic();
    }
    else
    {
        int stemWidth = m_doc->GetDrawingStemWidth(staff->m_drawingStaffSize);
        int strokeWidth = 2.8 * stemWidth;

        int shape = LIGATURE_DEFAULT;
        if (note->GetActualDur() != DUR_BR)
        {
            bool up = false;
            // Mensural notes have no Stem child - rely on the MEI @stem.dir
            if (note->GetStemDir() != STEMDIRECTION_NONE)
            {
                up = (note->GetStemDir() == STEMDIRECTION_up);
            }
            else if (staff->m_drawingNotationType == NOTATIONTYPE_NONE
                     || staff->m_drawingNotationType == NOTATIONTYPE_cmn)
            {
                up = (note->GetDrawingStemDir() == STEMDIRECTION_up);
            }
            shape = (up) ? LIGATURE_STEM_RIGHT_UP : LIGATURE_STEM_RIGHT_DOWN;
        }

        Point topLeft, bottomRight;
        int sides[4];
        this->CalcBrevisPoints(note, staff, &topLeft, &bottomRight, sides, shape, isMensuralBlack);

        dc->StartCustomGraphic("notehead");

        if (!fillNotehead) {
            // double the bases of rectangles
            DrawObliquePolygon(dc, topLeft.x + stemWidth, topLeft.y, bottomRight.x - stemWidth, topLeft.y, -strokeWidth);
            DrawObliquePolygon(
                dc, topLeft.x + stemWidth, bottomRight.y, bottomRight.x - stemWidth, bottomRight.y, strokeWidth);
        }
        else {
            DrawFilledRectangle(dc, topLeft.x + stemWidth, topLeft.y, bottomRight.x - stemWidth, bottomRight.y);
        }
        if (note->FindDescendantByType(PLICA)) {
            // Right side is a stem - end the notehead first
            dc->EndCustomGraphic();
            return;
        }
        // serifs and / or stem
        DrawFilledRectangle(dc, topLeft.x, sides[0], topLeft.x + stemWidth, sides[1]);

        if (note->GetActualDur() != DUR_BR) {
            // Right side is a stem - end the notehead first
            dc->EndCustomGraphic();
            dc->StartCustomGraphic("stem");
            DrawFilledRectangle(dc, bottomRight.x - stemWidth, sides[2], bottomRight.x, sides[3]);
            dc->EndCustomGraphic();
        }
        else {
            // Right side is a serif
            DrawFilledRectangle(dc, bottomRight.x - stemWidth, sides[2], bottomRight.x, sides[3]);
            dc->EndCustomGraphic();
        }
    }
}

void View::DrawLigature(DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff, Measure *measure)
{
    assert(dc);
    assert(element);
    assert(layer);
    assert(staff);

    Ligature *ligature = vrv_cast<Ligature *>(element);
    assert(ligature);

    dc->StartGraphic(ligature, "", ligature->GetUuid());

    // Draw children (notes)
    DrawLayerChildren(dc, ligature, layer, staff, measure);

    dc->EndGraphic(ligature, this);
}

void View::DrawLigatureNote(DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff)
{
    assert(dc);
    assert(element);
    assert(layer);
    assert(staff);

    Note *note = vrv_cast<Note *>(element);
    assert(note);

    Ligature *ligature = vrv_cast<Ligature *>(note->GetFirstAncestor(LIGATURE));
    assert(ligature);

    Note *prevNote = dynamic_cast<Note *>(ligature->GetListPrevious(note));
    Note *nextNote = dynamic_cast<Note *>(ligature->GetListNext(note));

    int position = ligature->GetListIndex(note);
    assert(position != -1);
    int shape = ligature->m_drawingShapes.at(position);
    int prevShape = (position > 0) ? ligature->m_drawingShapes.at(position - 1) : 0;

    /** code duplicated from View::DrawMaximaToBrevis */
    bool isMensuralBlack = (staff->m_drawingNotationType == NOTATIONTYPE_mensural_black);
    bool fillNotehead = (isMensuralBlack || note->GetColored()) && !(isMensuralBlack && note->GetColored());
    bool oblique = shape & LIGATURE_OBLIQUE || prevShape & LIGATURE_OBLIQUE;
    bool obliqueEnd = prevShape & LIGATURE_OBLIQUE;
    bool stackedEnd = shape & LIGATURE_STACKED;

    if ( m_doc->GetOptions()->m_useGlyphMensural.GetValue() )  // VITRY project phase II
    {
        int interval = nextNote? nextNote->GetPitchInterface()->PitchDifferenceTo( note->GetPitchInterface() ):0;
        int xNote = element->GetDrawingX();
        int yNote = element->GetDrawingY();
        int adv = element->GetDrawingXRel();
        dc->StartCustomGraphic("notehead");
        wchar_t code = -1;
        int step = 0;
        if ( shape & (LIGATURE_STEM_LEFT_UP|LIGATURE_STEM_LEFT_DOWN) )
        {
            wchar_t code = (shape & LIGATURE_STEM_LEFT_UP)? SMUFL_E93E_mensuralCombStemUp:SMUFL_E93F_mensuralCombStemDown;
            step += DrawSmuflCode(dc, xNote, yNote, code, staff->m_drawingStaffSize, false);
        }
        code = -1;
        if ( !obliqueEnd )
        {
            code = SMUFL_E952_mensuralBlackBrevis;
            if ( note->GetActualDur() == DUR_MX )
                code = SMUFL_E930_mensuralNoteheadMaximaBlack;
        }
        if ( oblique )
        {
            if ( !obliqueEnd )
            {
                switch (interval)
                {
                    case -1:
                        code = SMUFL_E980_mensuralObliqueDesc2ndBlack;
                        break;
                    case -2:
                        code = SMUFL_E984_mensuralObliqueDesc3rdBlack;
                        break;
                    case -3:
                        code = SMUFL_E988_mensuralObliqueDesc4thBlack;
                        break;
                    case -4:
                        code = SMUFL_E98C_mensuralObliqueDesc5thBlack;
                        break;
                    case -5:
                        code = SMUFL_F730_mensuralObliqueDesc6thBlack;
                        break;
                    case 1:
                        code = SMUFL_E970_mensuralObliqueAsc2ndBlack;
                        break;
                    case 2:
                        code = SMUFL_E974_mensuralObliqueAsc3rdBlack;
                        break;
                    case 3:
                        code = SMUFL_E978_mensuralObliqueAsc4thBlack;
                        break;
                    case 4:
                        code = SMUFL_E97C_mensuralObliqueAsc5thBlack;
                        break;
                }
            }
            else
            {
                if ( nextNote )
                    nextNote->SetDrawingXRel(note->GetDrawingXRel());
            }
        }
        if ( code != -1 )
        {
            step += DrawSmuflCode(dc, xNote, yNote, code, staff->m_drawingStaffSize, false);
        }
        if ( shape & (LIGATURE_STEM_RIGHT_UP|LIGATURE_STEM_RIGHT_DOWN) )
        {
            code = (shape & LIGATURE_STEM_RIGHT_UP)? SMUFL_E93E_mensuralCombStemUp:SMUFL_E93F_mensuralCombStemDown;
            step += DrawSmuflCode(dc, xNote+step, yNote, code, staff->m_drawingStaffSize, false);
        }
        if ( (!oblique || obliqueEnd) && fabs(interval) > 1 )
        {
            code = -1;
            if ( interval == -4 )
                code = SMUFL_F722_chantConnectingLineDesc5th;
            else if ( interval == -3 )
                code = SMUFL_F721_chantConnectingLineDesc4th;
            else if ( interval == -2 )
                code = SMUFL_F720_chantConnectingLineDesc3rd;
            if ( interval == 2 )
                code = SMUFL_E9BE_chantConnectingLineAsc3rd;
            else if ( interval == 3 )
                code = SMUFL_E9BF_chantConnectingLineAsc4th;
            else if ( interval == 4 )
                code = SMUFL_E9C0_chantConnectingLineAsc5th;
            if ( code != -1 )
            {
                step += DrawSmuflCode(dc, xNote+step, yNote, code, staff->m_drawingStaffSize, false);
            }
        }
        // NB:
        // xRel is the relative position towards the parent Ligature element (not the previous note!)
        // It is computed in functor-based CalcLigatureNotePos...
        //int xRel = element->GetDrawingXRel();
        adv += step;
        if ( nextNote )
            nextNote->SetDrawingXRel(adv);
        //font->SetWidthToHeightRatio(r);
        dc->EndCustomGraphic();
    }
    else
    {
        int stemWidth = m_doc->GetDrawingStemWidth(staff->m_drawingStaffSize);
        int strokeWidth = 2.8 * stemWidth;
        /** end code duplicated */
        
        Point points[4];
        Point *topLeft = &points[0];
        Point *bottomLeft = &points[1];
        Point *topRight = &points[2];
        Point *bottomRight = &points[3];
        int sides[4];
        if (!oblique) {
            this->CalcBrevisPoints(note, staff, topLeft, bottomRight, sides, shape, isMensuralBlack);
            bottomLeft->x = topLeft->x;
            bottomLeft->y = bottomRight->y;
            topRight->x = bottomRight->x;
            topRight->y = topLeft->y;
        }
        else {
            // First half of the oblique - checking the nextNote is there just in case, but is should
            if ((shape & LIGATURE_OBLIQUE) && nextNote) {
                // return;
                CalcObliquePoints(note, nextNote, staff, points, sides, shape, isMensuralBlack, true);
            }
            // Second half of the oblique - checking the prevNote is there just in case, but is should
            else if ((prevShape & LIGATURE_OBLIQUE) && prevNote) {
                CalcObliquePoints(prevNote, note, staff, points, sides, prevShape, isMensuralBlack, false);
            }
            else {
                assert(false);
            }
        }
        
        if (!fillNotehead) {
            // double the bases of rectangles
            DrawObliquePolygon(dc, topLeft->x, topLeft->y, topRight->x, topRight->y, -strokeWidth);
            DrawObliquePolygon(dc, bottomLeft->x, bottomLeft->y, bottomRight->x, bottomRight->y, strokeWidth);
        }
        else {
            DrawObliquePolygon(dc, topLeft->x, topLeft->y, topRight->x, topRight->y, bottomLeft->y - topLeft->y);
        }
        
        // Do not draw a left connector with obliques
        if (!obliqueEnd) {
            int sideTop = sides[0];
            int sideBottom = sides[1];
            if (prevNote) {
                Point prevTopLeft = *topLeft;
                Point prevBottomRight = *bottomRight;
                int prevSides[4];
                memcpy(prevSides, sides, 4 * sizeof(int));
                CalcBrevisPoints(prevNote, staff, &prevTopLeft, &prevBottomRight, prevSides, prevShape, isMensuralBlack);
                if (!stackedEnd) {
                    sideTop = std::max(sides[0], prevSides[2]);
                    sideBottom = std::min(sides[1], prevSides[3]);
                }
                else {
                    // Stacked end - simply use the bottom right [3] note since the interval is going up anyway
                    sides[3] = prevSides[3];
                }
            }
            DrawFilledRoundedRectangle(dc, topLeft->x, sideTop, topLeft->x + stemWidth, sideBottom, stemWidth / 3);
        }
        
        if (!nextNote) {
            DrawFilledRoundedRectangle(dc, bottomRight->x - stemWidth, sides[2], bottomRight->x, sides[3], stemWidth / 3);
        }
    }
}

void View::DrawDotInLigature(DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff, Measure *measure)
{
    assert(dc);
    assert(element);
    assert(layer);
    assert(staff);

    Dot *dot = vrv_cast<Dot *>(element);
    assert(dot);

    assert(dot->m_drawingPreviousElement && dot->m_drawingPreviousElement->Is(NOTE));
    Note *note = vrv_cast<Note *>(dot->m_drawingPreviousElement);
    assert(note);

    Ligature *ligature = vrv_cast<Ligature *>(note->GetFirstAncestor(LIGATURE));
    assert(ligature);

    bool favorGlyph = m_doc->GetOptions()->m_useGlyphMensural.GetValue();
    int position = ligature->GetListIndex(note);
    if ( favorGlyph )
        position--;
    assert(position != -1);
    int shape = ligature->m_drawingShapes.at(position);
    bool isLast = (position == (int)ligature->m_drawingShapes.size() - 1);

    int y = note->GetDrawingY();
    int x = note->GetDrawingX();
    /*
    if (!isLast && (shape & LIGATURE_OBLIQUE)) {
        x += note->GetDrawingRadius(m_doc, true);
        y += m_doc->GetDrawingUnit(staff->m_drawingStaffSize);
    }
    else */{
        if (!(shape&LIGATURE_OBLIQUE))
            x += note->GetDrawingRadius(m_doc, true)*2;
        //x += note->GetDrawingRadius(m_doc, true)*2;
        x += m_doc->GetDrawingUnit(staff->m_drawingStaffSize)*1/4;
        if ( staff->IsOnStaffLine(y, m_doc) )
            y -= m_doc->GetDrawingUnit(staff->m_drawingStaffSize)*2/3;
    }
    if ( favorGlyph )
    {
        wchar_t code = SMUFL_E1E7_augmentationDot;
        dc->StartCustomGraphic("dot");
        DrawSmuflCode( dc, x, y, code, staff->m_drawingStaffSize, false, true );
        dc->EndCustomGraphic();
    }
    else
    {
        DrawDotsPart(dc, x, y, 1, staff);
    }
}

void View::DrawPlica(DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff, Measure *measure)
{
    assert(dc);
    assert(element);
    assert(layer);
    assert(staff);

    Plica *plica = vrv_cast<Plica *>(element);
    assert(plica);

    Note *note = vrv_cast<Note *>(plica->GetFirstAncestor(NOTE));
    assert(note);

    bool isLonga = (note->GetActualDur() == DUR_LG);
    bool up = (plica->GetDir() == STEMDIRECTION_basic_up);
    
    if ( m_doc->GetOptions()->m_useGlyphMensural.GetValue() )
    {
        const int yNote = element->GetDrawingY();
        const int xNote = element->GetDrawingX();
        wchar_t code = -1;
        if ( isLonga )
        {
            if ( up )
            {
                code = SMUFL_F710_plicaBlackLongaAsc;
            }
            else
            {
                code = SMUFL_F711_plicaBlackLongaDesc;
            }
        }
        else    //brevis
        {
            if ( up )
            {
                if (note->IsInLigature()) {
                    code = SMUFL_E93E_mensuralCombStemUp;
                }
                else {
                    code = SMUFL_F712_plicaBlackBrevisAsc;
                }
            }
            else
            {
                if (note->IsInLigature()) {
                    code = SMUFL_E93F_mensuralCombStemDown;
                }
                else {
                    code = SMUFL_F713_plicaBlackBrevisDesc;
                }
            }
        }
        dc->StartCustomGraphic("notehead");
        DrawSmuflCode( dc, xNote, yNote, code, staff->m_drawingStaffSize, false, true );
        dc->EndCustomGraphic();
    }
    else
    {
        dc->StartGraphic(plica, "", plica->GetUuid());
        bool isMensuralBlack = (staff->m_drawingNotationType == NOTATIONTYPE_mensural_black);
        int stemWidth = m_doc->GetDrawingStemWidth(staff->m_drawingStaffSize);
        int shape = LIGATURE_DEFAULT;
        Point topLeft, bottomRight;
        int sides[4];
        this->CalcBrevisPoints(note, staff, &topLeft, &bottomRight, sides, shape, isMensuralBlack);
        int stem = m_doc->GetDrawingUnit(staff->m_drawingStaffSize);
        stem *= (!isMensuralBlack) ? 7 : 5;
        int shortStem = m_doc->GetDrawingUnit(staff->m_drawingStaffSize);
        shortStem *= (!isMensuralBlack) ? 3.5 : 2.5;
        if (isLonga) {
            if (up) {
                DrawFilledRectangle(dc, topLeft.x, sides[1], topLeft.x + stemWidth, sides[1] + shortStem);
                DrawFilledRectangle(dc, bottomRight.x, sides[1], bottomRight.x - stemWidth, sides[1] + stem);
            }
            else {
                DrawFilledRectangle(dc, topLeft.x, sides[0], topLeft.x + stemWidth, sides[0] - shortStem);
                DrawFilledRectangle(dc, bottomRight.x, sides[0], bottomRight.x - stemWidth, sides[0] - stem);
            }
        }
        // brevis
        else {
            if (up) {
                DrawFilledRectangle(dc, topLeft.x, sides[1], topLeft.x + stemWidth, sides[1] + stem);
                DrawFilledRectangle(dc, bottomRight.x, sides[1], bottomRight.x - stemWidth, sides[1] + shortStem);
            }
            else {
                DrawFilledRectangle(dc, topLeft.x, sides[0], topLeft.x + stemWidth, sides[0] - stem);
                DrawFilledRectangle(dc, bottomRight.x, sides[0], bottomRight.x - stemWidth, sides[0] - shortStem);
            }
        }
        dc->EndGraphic(plica, this);
    }
}

void View::DrawProportFigures(DeviceContext *dc, int x, int y, int num, int numBase, Staff *staff)
{
    assert(dc);
    assert(staff);

    int ynum = 0, yden = 0;
    int textSize = staff->m_drawingStaffSize;
    std::wstring wtext;

    if (numBase) {
        ynum = y + m_doc->GetDrawingDoubleUnit(textSize);
        yden = y - m_doc->GetDrawingDoubleUnit(textSize);
    }
    else
        ynum = y;

    if (numBase > 9 || num > 9) {
        x += m_doc->GetDrawingUnit(textSize) * 2;
    }

    dc->SetFont(m_doc->GetDrawingSmuflFont(textSize, false));

    wtext = IntToTimeSigFigures(num);
    DrawSmuflString(dc, x, ynum, wtext, HORIZONTALALIGNMENT_center, textSize); // true = center

    if (numBase) {
        wtext = IntToTimeSigFigures(numBase);
        DrawSmuflString(dc, x, yden, wtext, HORIZONTALALIGNMENT_center, textSize); // true = center
    }

    dc->ResetFont();
}

void View::DrawProport(DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff, Measure *measure)
{
    assert(layer);
    assert(staff);
    assert(dynamic_cast<Proport *>(element)); // Element must be a Proport"

    int x1, x2, y1, y2;

    Proport *proport = dynamic_cast<Proport *>(element);

    dc->StartGraphic(element, "", element->GetUuid());

    int y = staff->GetDrawingY() - (m_doc->GetDrawingUnit(staff->m_drawingStaffSize) * 4);
    int x = element->GetDrawingX();

    x1 = x + 120;
    x2 = x1 + 150; // ??TEST: JUST DRAW AN ARBITRARY RECTANGLE
    y1 = y;
    y2 = y + 50 + (50 * proport->GetNum());
    // DrawFilledRectangle(dc,x1,y1,x2,y2);
    DrawPartFilledRectangle(dc, x1, y1, x2, y2, 0);

    if (proport->HasNum()) {
        x = element->GetDrawingX();
        // if (proport->GetSign() || proport->HasTempus())           // ??WHAT SHOULD THIS BE?
        {
            x += m_doc->GetDrawingUnit(staff->m_drawingStaffSize)
                * 5; // step forward because we have a sign or a meter symbol
        }
        int numbase = proport->HasNumbase() ? proport->GetNumbase() : 0;
        DrawProportFigures(dc, x,
            staff->GetDrawingY() - m_doc->GetDrawingUnit(staff->m_drawingStaffSize) * (staff->m_drawingLines - 1),
            proport->GetNum(), numbase, staff);
    }

    dc->EndGraphic(element, this);
}

void View::CalcBrevisPoints(
    Note *note, Staff *staff, Point *topLeft, Point *bottomRight, int sides[4], int shape, bool isMensuralBlack)
{
    assert(note);
    assert(staff);
    assert(topLeft);
    assert(bottomRight);

    int y = note->GetDrawingY();

    // Calculate size of the rectangle
    topLeft->x = note->GetDrawingX();
    const int width = 2 * note->GetDrawingRadius(m_doc, true);
    bottomRight->x = topLeft->x + width;

    double heightFactor = (isMensuralBlack) ? 0.8 : 1.0;
    topLeft->y = y + m_doc->GetDrawingUnit(staff->m_drawingStaffSize) * heightFactor;
    bottomRight->y = y - m_doc->GetDrawingUnit(staff->m_drawingStaffSize) * heightFactor;

    sides[0] = topLeft->y;
    sides[1] = bottomRight->y;

    if (!isMensuralBlack) {
        // add sherif
        sides[0] += (int)m_doc->GetDrawingUnit(staff->m_drawingStaffSize) / 3;
        sides[1] -= (int)m_doc->GetDrawingUnit(staff->m_drawingStaffSize) / 3;
    }
    else if (shape & LIGATURE_OBLIQUE) {
        // shorten the sides to make sure they are note visible with oblique ligatures
        sides[0] -= (int)m_doc->GetDrawingUnit(staff->m_drawingStaffSize) / 2;
        sides[1] += (int)m_doc->GetDrawingUnit(staff->m_drawingStaffSize) / 2;
    }

    sides[2] = sides[0];
    sides[3] = sides[1];

    int stem = m_doc->GetDrawingUnit(staff->m_drawingStaffSize);
    stem *= (!isMensuralBlack) ? 7 : 5;

    if (shape & LIGATURE_STEM_LEFT_UP) sides[0] = y + stem;
    if (shape & LIGATURE_STEM_LEFT_DOWN) sides[1] = y - stem;
    if (shape & LIGATURE_STEM_RIGHT_UP) sides[2] = y + stem;
    if (shape & LIGATURE_STEM_RIGHT_DOWN) sides[3] = y - stem;
}

void View::CalcObliquePoints(Note *note1, Note *note2, Staff *staff, Point points[4], int sides[4], int shape,
    bool isMensuralBlack, bool firstHalf)
{
    assert(note1);
    assert(note2);
    assert(staff);

    const int stemWidth = m_doc->GetDrawingStemWidth(staff->m_drawingStaffSize);

    Point *topLeft = &points[0];
    Point *bottomLeft = &points[1];
    Point *topRight = &points[2];
    Point *bottomRight = &points[3];

    int sides1[4];
    CalcBrevisPoints(note1, staff, topLeft, bottomLeft, sides1, shape, isMensuralBlack);
    // Correct the x of bottomLeft
    bottomLeft->x = topLeft->x;
    // Copy the left sides
    sides[0] = sides1[0];
    sides[1] = sides1[1];

    int sides2[4];
    // add OBLIQUE shape to make sure sides are shortened in mensural black
    CalcBrevisPoints(note2, staff, topRight, bottomRight, sides2, LIGATURE_OBLIQUE, isMensuralBlack);
    // Correct the x of topRight;
    topRight->x = bottomRight->x;
    // Copy the right sides
    sides[2] = sides2[2];
    sides[3] = sides2[3];

    // With oblique it is best visually to move them up / down - more with (white) ligatures with serif
    double adjustmentFactor = (isMensuralBlack) ? 0.5 : 1.8;
    double slope = 0.0;
    if (bottomRight->x != bottomLeft->x)
        slope = (double)(bottomRight->y - bottomLeft->y) / (double)(bottomRight->x - bottomLeft->x);
    int adjustment = (int)(slope * stemWidth) * adjustmentFactor;
    topLeft->y -= adjustment;
    bottomLeft->y -= adjustment;
    topRight->y += adjustment;
    bottomRight->y += adjustment;

    slope = 0.0;
    // recalculate slope after adjustment
    if (bottomRight->x != bottomLeft->x)
        slope = (double)(bottomRight->y - bottomLeft->y) / (double)(bottomRight->x - bottomLeft->x);
    int length = (bottomRight->x - bottomLeft->x) / 2;

    if (firstHalf) {
        // make sure there are some pixels of overlap
        length += 10;
        bottomRight->x = bottomLeft->x + length;
        topRight->x = bottomRight->x;
        bottomRight->y = bottomLeft->y + (int)(length * slope);
        topRight->y = topLeft->y + (int)(length * slope);
    }
    else {
        bottomLeft->x = bottomLeft->x + length;
        topLeft->x = bottomLeft->x;
        bottomLeft->y = bottomLeft->y + (int)(length * slope);
        topLeft->y = topLeft->y + (int)(length * slope);
    }
}

} // namespace vrv
