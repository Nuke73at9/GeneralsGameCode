#!/usr/bin/env python3
"""
Abram's Personal Test: generates resources/abram/Window/Menus/ModSelectMenu.wnd

The Mods screen layout, written in the game's .wnd window layout format. It uses the game's own
image names (button and list box art) so it looks like the other menus. Layout coordinates are for
the 800x600 creation resolution and the engine scales them to the screen.

Run from the repository root:
    python3 scripts/abram/gen_mod_select_menu_wnd.py
"""

import os

PREFIX = "ModSelectMenu.wnd:"

NONE_ROW = "IMAGE: NoImage, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0"


def draw_data(key, rows, indent):
    """Writes one draw data block. Every block has exactly 9 rows; missing rows are empty."""
    rows = list(rows) + [NONE_ROW] * (9 - len(rows))
    pad = " " * indent
    lead = f"{pad}{key} = "
    cont = " " * len(lead)
    lines = [lead + rows[0] + ","]
    for row in rows[1:-1]:
        lines.append(cont + row + ",")
    lines.append(cont + rows[-1] + ";")
    return "\n".join(lines)


def text_color(indent, enabled="255 255 255 255", enabled_border="0 0 0 255",
               disabled="192 192 192 255", disabled_border="64 64 64 255",
               hilite="186 255 12 255", hilite_border="0 2 0 255"):
    pad = " " * indent
    return (f"{pad}TEXTCOLOR = ENABLED:  {enabled}, ENABLEDBORDER:  {enabled_border},\n"
            f"{pad}            DISABLED: {disabled}, DISABLEDBORDER: {disabled_border},\n"
            f"{pad}            HILITE:   {hilite}, HILITEBORDER:   {hilite_border};")


def header(window_type, rect, name, status, style, indent, system="[None]", input_cb="[None]",
           draw="[None]", font=("Arial", 10), header_template="[NONE]", text=None):
    pad = " " * indent
    (x1, y1), (x2, y2) = rect
    out = [
        f"{pad}WINDOWTYPE = {window_type};",
        f"{pad}SCREENRECT = UPPERLEFT: {x1} {y1},",
        f"{pad}             BOTTOMRIGHT: {x2} {y2},",
        f"{pad}             CREATIONRESOLUTION: 800 600;",
        f'{pad}NAME = "{PREFIX}{name}";',
        f"{pad}STATUS = {status};",
        f"{pad}STYLE = {style};",
        f'{pad}SYSTEMCALLBACK = "{system}";',
        f'{pad}INPUTCALLBACK = "{input_cb}";',
        f'{pad}TOOLTIPCALLBACK = "[None]";',
        f'{pad}DRAWCALLBACK = "{draw}";',
        f'{pad}FONT = NAME: "{font[0]}", SIZE: {font[1]}, BOLD: 0;',
        f'{pad}HEADERTEMPLATE = "{header_template}";',
        f"{pad}TOOLTIPDELAY = -1;",
    ]
    if text is not None:
        out.append(f'{pad}TEXT = "{text}";')
    return "\n".join(out)


def push_button(name, rect, indent=6):
    return "\n".join([
        header("PUSHBUTTON", rect, name, "ENABLED+IMAGE+NOFOCUS", "PUSHBUTTON+MOUSETRACK", indent,
               font=("Generals", 15), header_template="MainButton"),
        text_color(indent, disabled="62 64 92 255", disabled_border="31 32 47 255"),
        draw_data("ENABLEDDRAWDATA", [
            "IMAGE: Buttons-Left, COLOR: 255 0 0 255, BORDERCOLOR: 255 128 128 255",
            "IMAGE: NoImage, COLOR: 47 55 168 255, BORDERCOLOR: 254 254 254 255",
            NONE_ROW, NONE_ROW, NONE_ROW,
            "IMAGE: Buttons-Middle, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
            "IMAGE: Buttons-Right, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
        ], indent),
        draw_data("DISABLEDDRAWDATA", [
            "IMAGE: Buttons-Disabled-Left, COLOR: 128 128 128 255, BORDERCOLOR: 192 192 192 255",
            "IMAGE: NoImage, COLOR: 192 192 192 255, BORDERCOLOR: 128 128 128 255",
            NONE_ROW, NONE_ROW, NONE_ROW,
            "IMAGE: Buttons-Disabled-Middle, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
            "IMAGE: Buttons-Disabled-Right, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
        ], indent),
        draw_data("HILITEDRAWDATA", [
            "IMAGE: Buttons-HiLite-Left, COLOR: 209 253 4 255, BORDERCOLOR: 59 60 52 255",
            "IMAGE: Buttons-Pushed-Left, COLOR: 47 55 168 255, BORDERCOLOR: 254 254 254 255",
            NONE_ROW,
            "IMAGE: Buttons-Pushed-Middle, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
            "IMAGE: Buttons-Pushed-Right, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
            "IMAGE: Buttons-HiLite-Middle, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
            "IMAGE: Buttons-HiLite-Right, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
        ], indent),
    ])


def static_text(name, rect, font, header_template, centered, indent=6):
    clear = "IMAGE: NoImage, COLOR: 0 0 0 0, BORDERCOLOR: 0 0 0 0"
    pad = " " * indent
    return "\n".join([
        header("STATICTEXT", rect, name, "ENABLED+NOINPUT+NOFOCUS", "STATICTEXT", indent,
               font=font, header_template=header_template),
        text_color(indent, hilite="255 255 255 255", hilite_border="0 0 0 255"),
        draw_data("ENABLEDDRAWDATA", [clear], indent),
        draw_data("DISABLEDDRAWDATA", [clear], indent),
        draw_data("HILITEDRAWDATA", [clear], indent),
        f"{pad}STATICTEXTDATA = CENTERED: {1 if centered else 0};",
    ])


def listbox(name, rect, indent=6):
    pad = " " * indent
    return "\n".join([
        header("SCROLLLISTBOX", rect, name, "ENABLED", "SCROLLLISTBOX+MOUSETRACK", indent,
               font=("Arial", 12), header_template="LabelRegular"),
        text_color(indent, enabled="254 254 254 255", hilite="128 128 255 255", hilite_border="0 0 128 255"),
        draw_data("ENABLEDDRAWDATA", [
            "IMAGE: BlackSquare, COLOR: 0 0 0 126, BORDERCOLOR: 49 55 168 255",
            "IMAGE: ListBoxHiliteItemLeftEnd, COLOR: 255 255 0 255, BORDERCOLOR: 254 254 254 255",
            "IMAGE: ListBoxHiliteItemRightEnd, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
            "IMAGE: ListBoxHiliteItemRepeatingCenter, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
            "IMAGE: ListBoxHiliteItemSmallRepeatingCenter, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
        ], indent),
        draw_data("DISABLEDDRAWDATA", [
            "IMAGE: NoImage, COLOR: 255 4 0 0, BORDERCOLOR: 49 55 168 255",
            "IMAGE: NoImage, COLOR: 192 192 192 255, BORDERCOLOR: 254 254 254 255",
        ], indent),
        draw_data("HILITEDRAWDATA", [
            "IMAGE: BlackSquare, COLOR: 0 0 0 126, BORDERCOLOR: 49 55 168 255",
            "IMAGE: ListBoxHiliteSelectedItemLeftEnd, COLOR: 254 254 254 255, BORDERCOLOR: 0 128 0 255",
            "IMAGE: ListBoxHiliteSelectedItemRightEnd, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
            "IMAGE: ListBoxHiliteSelectedItemRepeatingCenter, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
            "IMAGE: ListBoxHiliteSelectedItemSmallRepeatingCenter, COLOR: 255 255 255 0, BORDERCOLOR: 255 255 255 0",
        ], indent),
        f"{pad}LISTBOXDATA = LENGTH: 200,",
        f"{pad}              AUTOSCROLL: 0,",
        f"{pad}              SCROLLIFATEND: 0,",
        f"{pad}              AUTOPURGE: 0,",
        f"{pad}              SCROLLBAR: 1,",
        f"{pad}              MULTISELECT: 0,",
        f"{pad}              COLUMNS: 1,",
        f"{pad}              FORCESELECT: 1;",
        draw_data("LISTBOXENABLEDUPBUTTONDRAWDATA", [
            "IMAGE: VSliderUpButtonEnabled, COLOR: 255 0 0 255, BORDERCOLOR: 255 128 128 255",
            "IMAGE: VSliderUpButtonHiliteSelected, COLOR: 255 255 0 255, BORDERCOLOR: 254 254 254 255",
        ], indent),
        draw_data("LISTBOXDISABLEDUPBUTTONDRAWDATA", [
            "IMAGE: VSliderUpButtonDisabled, COLOR: 128 128 128 255, BORDERCOLOR: 192 192 192 255",
            "IMAGE: NoImage, COLOR: 192 192 192 255, BORDERCOLOR: 254 254 254 255",
        ], indent),
        draw_data("LISTBOXHILITEUPBUTTONDRAWDATA", [
            "IMAGE: VSliderUpButtonHilite, COLOR: 0 255 0 255, BORDERCOLOR: 0 128 0 255",
            "IMAGE: VSliderUpButtonHiliteSelected, COLOR: 254 254 254 255, BORDERCOLOR: 0 128 0 255",
        ], indent),
        draw_data("LISTBOXENABLEDDOWNBUTTONDRAWDATA", [
            "IMAGE: VSliderDownButtonEnabled, COLOR: 255 0 0 255, BORDERCOLOR: 255 128 128 255",
            "IMAGE: VSliderDownButtonHiliteSelected, COLOR: 255 255 0 255, BORDERCOLOR: 254 254 254 255",
        ], indent),
        draw_data("LISTBOXDISABLEDDOWNBUTTONDRAWDATA", [
            "IMAGE: VSliderDownButtonDisabled, COLOR: 128 128 128 255, BORDERCOLOR: 192 192 192 255",
            "IMAGE: NoImage, COLOR: 192 192 192 255, BORDERCOLOR: 254 254 254 255",
        ], indent),
        draw_data("LISTBOXHILITEDOWNBUTTONDRAWDATA", [
            "IMAGE: VSliderDownButtonHilite, COLOR: 0 255 0 255, BORDERCOLOR: 0 128 0 255",
            "IMAGE: VSliderDownButtonHiliteSelected, COLOR: 254 254 254 255, BORDERCOLOR: 0 128 0 255",
        ], indent),
        draw_data("LISTBOXENABLEDSLIDERDRAWDATA", [
            "IMAGE: NoImage, COLOR: 255 190 0 0, BORDERCOLOR: 47 55 168 255",
        ], indent),
        draw_data("LISTBOXDISABLEDSLIDERDRAWDATA", [
            "IMAGE: NoImage, COLOR: 128 128 128 0, BORDERCOLOR: 148 112 0 255",
        ], indent),
        draw_data("LISTBOXHILITESLIDERDRAWDATA", [
            "IMAGE: NoImage, COLOR: 0 255 0 0, BORDERCOLOR: 49 55 168 255",
        ], indent),
        draw_data("SLIDERTHUMBENABLEDDRAWDATA", [
            "IMAGE: ScrollBarThumbEnabled, COLOR: 255 4 0 0, BORDERCOLOR: 255 243 28 255",
            "IMAGE: ScrollBarThumbHiliteSelected, COLOR: 255 255 0 255, BORDERCOLOR: 254 254 254 255",
        ], indent),
        draw_data("SLIDERTHUMBDISABLEDDRAWDATA", [
            "IMAGE: ScrollBarThumbDisabled, COLOR: 128 128 128 255, BORDERCOLOR: 192 192 192 255",
            "IMAGE: NoImage, COLOR: 192 192 192 255, BORDERCOLOR: 254 254 254 255",
        ], indent),
        draw_data("SLIDERTHUMBHILITEDRAWDATA", [
            "IMAGE: ScrollBarThumbHilite, COLOR: 0 255 0 255, BORDERCOLOR: 0 128 0 255",
            "IMAGE: ScrollBarThumbHiliteSelected, COLOR: 254 254 254 255, BORDERCOLOR: 0 128 0 255",
        ], indent),
    ])


def preview_window(name, rect, indent=6):
    """A plain window that shows the mod's picture as its image, with a thin blue frame."""
    frame = "IMAGE: NoImage, COLOR: 0 0 0 255, BORDERCOLOR: 49 55 168 255"
    return "\n".join([
        header("USER", rect, name, "ENABLED+NOINPUT+NOFOCUS", "USER", indent),
        text_color(indent),
        draw_data("ENABLEDDRAWDATA", [frame], indent),
        draw_data("DISABLEDDRAWDATA", [frame], indent),
        draw_data("HILITEDRAWDATA", [frame], indent),
    ])


def window(body, children, indent):
    pad = " " * indent
    parts = [f"{pad}WINDOW", body]
    for child in children:
        parts.append(f"{pad}  CHILD")
        parts.append(child)
    if children:
        parts.append(f"{pad}  ENDALLCHILDREN")
    parts.append(f"{pad}END")
    return "\n".join(parts)


def build():
    # Screen frame and panel, same placement as the game's map select screen.
    parent_body = "\n".join([
        header("USER", ((0, 0), (800, 599)), "ModSelectMenuParent", "ENABLED+IMAGE+NOFOCUS", "USER", 2,
               system="ModSelectMenuSystem", input_cb="ModSelectMenuInput", font=("Times New Roman", 14)),
        text_color(2),
        draw_data("ENABLEDDRAWDATA", ["IMAGE: MainMenuRuler, COLOR: 0 0 128 255, BORDERCOLOR: 254 254 254 255"], 2),
        draw_data("DISABLEDDRAWDATA", ["IMAGE: NoImage, COLOR: 64 64 64 255, BORDERCOLOR: 254 254 254 255"], 2),
        draw_data("HILITEDRAWDATA", ["IMAGE: NoImage, COLOR: 128 128 255 255, BORDERCOLOR: 254 254 254 255"], 2),
    ])

    panel = "IMAGE: NoImage, COLOR: 2 2 2 193, BORDERCOLOR: 47 55 168 255"
    sub_body = "\n".join([
        header("USER", ((84, 84), (714, 518)), "SubParent", "ENABLED", "USER", 4,
               system="PassMessagesToParentSystem"),
        text_color(4),
        draw_data("ENABLEDDRAWDATA", [panel], 4),
        draw_data("DISABLEDDRAWDATA", [panel], 4),
        draw_data("HILITEDRAWDATA", [panel], 4),
    ])

    # Texts are filled in by code (ModSelectMenu.cpp), so no string table entries are needed.
    sub_children = [
        window(static_text("StaticTextTitle", ((98, 96), (700, 128)), ("Generals", 20), "Title", True), [], 4),
        window(listbox("ListboxMods", ((112, 140), (392, 444))), [], 4),
        window(preview_window("WinModPreview", ((408, 140), (686, 280))), [], 4),
        window(static_text("StaticTextModName", ((408, 288), (686, 312)), ("Arial", 14), "MinorTitle", False), [], 4),
        window(static_text("StaticTextModInfo", ((408, 316), (686, 444)), ("Arial", 11), "LabelRegular", False), [], 4),
        window(static_text("StaticTextStatus", ((112, 450), (686, 470)), ("Arial", 11), "LabelRegular", True), [], 4),
        window(push_button("ButtonActivate", ((112, 476), (280, 504))), [], 4),
        window(push_button("ButtonBack", ((518, 476), (686, 504))), [], 4),
    ]

    sub = window(sub_body, sub_children, 2)
    root = window(parent_body, [sub], 0)

    return "\n".join([
        "FILE_VERSION = 2;",
        "STARTLAYOUTBLOCK",
        "  LAYOUTINIT = ModSelectMenuInit;",
        "  LAYOUTUPDATE = ModSelectMenuUpdate;",
        "  LAYOUTSHUTDOWN = ModSelectMenuShutdown;",
        "ENDLAYOUTBLOCK",
        root,
        "",
    ])


if __name__ == "__main__":
    out_path = os.path.join("resources", "abram", "Window", "Menus", "ModSelectMenu.wnd")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", newline="\r\n") as f:
        f.write(build())
    print("Wrote", out_path)
