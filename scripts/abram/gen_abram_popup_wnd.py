#!/usr/bin/env python3
"""
Abram's Personal Test: generates resources/abram/Window/Menus/AbramPopup.wnd

A small dialog used by the Match Options screen and the in-game team menu: a title, a message line and
up to eight buttons. The code (AbramPopup.cpp) fills in the texts, hides the unused buttons and handles
the clicks. Reuses the window writers of gen_mod_select_menu_wnd.py so it looks like the other menus.

Run from the repository root:
    python3 scripts/abram/gen_abram_popup_wnd.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_mod_select_menu_wnd as wnd  # noqa: E402

wnd.PREFIX = "AbramPopup.wnd:"

MAX_CHOICES = 8


def build():
    # A transparent full screen parent catches the clicks around the dialog, so the screen below waits.
    clear = "IMAGE: NoImage, COLOR: 0 0 0 0, BORDERCOLOR: 0 0 0 0"
    parent_body = "\n".join([
        wnd.header("USER", ((0, 0), (800, 599)), "AbramPopupParent", "ENABLED+NOFOCUS", "USER", 2,
                   system="AbramPopupSystem", input_cb="AbramPopupInput", font=("Times New Roman", 14)),
        wnd.text_color(2),
        wnd.draw_data("ENABLEDDRAWDATA", [clear], 2),
        wnd.draw_data("DISABLEDDRAWDATA", [clear], 2),
        wnd.draw_data("HILITEDRAWDATA", [clear], 2),
    ])

    panel = "IMAGE: NoImage, COLOR: 2 2 2 220, BORDERCOLOR: 47 55 168 255"
    sub_body = "\n".join([
        wnd.header("USER", ((240, 110), (560, 486)), "SubParent", "ENABLED", "USER", 4,
                   system="PassMessagesToParentSystem"),
        wnd.text_color(4),
        wnd.draw_data("ENABLEDDRAWDATA", [panel], 4),
        wnd.draw_data("DISABLEDDRAWDATA", [panel], 4),
        wnd.draw_data("HILITEDRAWDATA", [panel], 4),
    ])

    children = [
        wnd.window(wnd.static_text("StaticTextTitle", ((250, 118), (550, 148)), ("Generals", 18), "Title", True), [], 4),
        wnd.window(wnd.static_text("StaticTextMessage", ((256, 152), (544, 196)), ("Arial", 11), "LabelRegular", True), [], 4),
    ]
    for i in range(MAX_CHOICES):
        top = 202 + i * 34
        children.append(wnd.window(wnd.push_button(f"ButtonChoice{i}", ((262, top), (538, top + 28))), [], 4))

    sub = wnd.window(sub_body, children, 2)
    root = wnd.window(parent_body, [sub], 0)

    return "\n".join([
        "FILE_VERSION = 2;",
        "STARTLAYOUTBLOCK",
        "  LAYOUTINIT = AbramPopupInit;",
        "  LAYOUTUPDATE = [None];",
        "  LAYOUTSHUTDOWN = [None];",
        "ENDLAYOUTBLOCK",
        root,
        "",
    ])


if __name__ == "__main__":
    out_path = os.path.join("resources", "abram", "Window", "Menus", "AbramPopup.wnd")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", newline="\r\n") as f:
        f.write(build())
    print("Wrote", out_path)
