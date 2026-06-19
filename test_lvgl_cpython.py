#!/usr/bin/env python3
"""Smoke tests for the CPython lvgl extension."""

import sys


def _fail(msg):
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def test_import_and_constants(lv):
    lv.init()
    assert hasattr(lv, "deinit")
    lv.deinit()
    print("OK: import lvgl; init/deinit")


def test_string_constants(lv):
    for name in ("SYMBOL_OK", "SYMBOL_CLOSE", "SYMBOL_HOME"):
        if not hasattr(lv, name):
            _fail(f"missing string constant {name}")
    print("OK: LVGL string constants present")


def test_enums(lv):
    if lv.EVENT.CLICKED != 7:
        _fail("lv.EVENT.CLICKED unexpected value")
    print("OK: enum namespaces (lv.EVENT.CLICKED)")


def test_widget_types(lv):
    for name in ("obj", "label", "button"):
        if not hasattr(lv, name):
            _fail(f"missing widget type lv.{name}")
    print("OK: widget types registered (lv.obj, lv.label, …)")


def test_module_functions(lv):
    for name in ("display_create", "screen_active", "tick_inc"):
        if not hasattr(lv, name):
            _fail(f"missing module function lv.{name}")
    print("OK: module functions (display_create, screen_active, …)")


def _setup_display(lv):
    disp = lv.display_create(240, 240)
    lv.display_set_color_format(disp, lv.COLOR_FORMAT.RGB565)
    buf = lv.draw_buf_create(240, 240, lv.COLOR_FORMAT.RGB565, 0)
    lv.display_set_draw_buffers(disp, buf, None)
    lv.display_set_render_mode(disp, lv.DISPLAY_RENDER_MODE.PARTIAL)
    return disp


def test_widget(lv):
    scr = lv.screen_active()
    label = lv.label(scr)
    label.set_text("cmods smoke")
    if label.get_text() != "cmods smoke":
        _fail("label.get_text() mismatch")
    print("OK: label create/set_text on active screen")


def test_callbacks(lv):
    scr = lv.screen_active()
    screen_fired = []

    def on_screen_clicked(event):
        screen_fired.append(event.get_code())

    scr.add_event_cb(on_screen_clicked, lv.EVENT.CLICKED, None)
    scr.send_event(lv.EVENT.CLICKED, None)
    if not screen_fired:
        _fail("screen CLICKED callback did not run")

    btn = lv.button(scr)
    btn.set_size(80, 40)
    btn_fired = []

    def on_button_clicked(event):
        btn_fired.append(1)

    btn.add_event_cb(on_button_clicked, lv.EVENT.CLICKED, None)
    btn.send_event(lv.EVENT.CLICKED, None)
    if not btn_fired:
        _fail("button CLICKED callback did not run")
    print("OK: screen and button event callbacks")


def test_event_callback(lv):
    test_callbacks(lv)


def test_button_callback(lv):
    pass


def main():
    import lvgl as lv

    test_import_and_constants(lv)
    test_string_constants(lv)
    test_enums(lv)
    test_widget_types(lv)
    test_module_functions(lv)

    lv.init()
    try:
        _setup_display(lv)
        test_widget(lv)
        test_callbacks(lv)
    finally:
        lv.deinit()

    print("All CPython lvgl smoke tests passed.")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise
