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
    for name in ("display_create", "screen_active", "tick_inc", "refr_now"):
        if not hasattr(lv, name):
            _fail(f"missing module function lv.{name}")
    print("OK: module functions (display_create, screen_active, …)")


def test_refr_now(lv):
    disp = lv.display_create(80, 80)
    buf = lv.draw_buf_create(80, 8, lv.COLOR_FORMAT.RGB565, 0)
    lv.display_set_draw_buffers(disp, buf, None)
    lv.display_set_render_mode(disp, lv.DISPLAY_RENDER_MODE.PARTIAL)
    disp.set_flush_cb(lambda d, area, color_p: d.flush_ready())
    before = lv.display_get_default()
    lv.refr_now(disp)
    after = lv.display_get_default()
    if before is None or after is None:
        _fail("display_get_default() returned None around refr_now")
    if lv.screen_active() is None:
        _fail("screen_active() returned None after refr_now")
    print("OK: refr_now refreshes without deleting the display")


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


def test_blob_dereference(lv):
    disp = lv.display_create(16, 16)
    buf = lv.draw_buf_create(16, 4, lv.COLOR_FORMAT.RGB565, 0)
    lv.display_set_draw_buffers(disp, buf, None)
    seen = []

    def flush_cb(d, area, color_p):
        width = area.x2 - area.x1 + 1
        height = area.y2 - area.y1 + 1
        data = color_p.__dereference__(width * height * 2)
        seen.append(len(data))
        d.flush_ready()

    disp.set_flush_cb(flush_cb)
    lv.refr_now(disp)
    if not seen:
        _fail("flush callback did not run during refr_now")
    print("OK: Blob.__dereference__ in flush callback")


def test_nesting(lv):
    if not hasattr(lv, "_nesting"):
        _fail("missing lv._nesting")
    if not hasattr(lv._nesting, "value"):
        _fail("lv._nesting missing value attribute")
    print("OK: lv._nesting present")


def main():
    import lvgl as lv

    test_import_and_constants(lv)
    test_string_constants(lv)
    test_enums(lv)
    test_widget_types(lv)
    test_module_functions(lv)
    test_nesting(lv)

    lv.init()
    try:
        _setup_display(lv)
        test_refr_now(lv)
        test_blob_dereference(lv)
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
