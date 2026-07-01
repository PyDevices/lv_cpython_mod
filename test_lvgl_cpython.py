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
    for name in ("OK", "CLOSE", "HOME"):
        if not hasattr(lv.SYMBOL, name):
            _fail(f"missing lv.SYMBOL.{name}")
    print("OK: LVGL SYMBOL namespace (lv.SYMBOL.OK, …)")


def test_enums(lv):
    clicked = lv.EVENT.CLICKED
    if not isinstance(clicked, int) or clicked <= 0:
        _fail(f"lv.EVENT.CLICKED unexpected value: {clicked!r}")
    if not hasattr(lv.obj, "FLAG"):
        _fail("lv.obj missing FLAG enum namespace")
    if lv.obj.FLAG.SCROLLABLE != (1 << 4):
        _fail("lv.obj.FLAG.SCROLLABLE unexpected value")
    if hasattr(lv, "OBJ_FLAG"):
        _fail("lv.OBJ_FLAG must not be exposed at module level")
    if not hasattr(lv.label, "LONG_MODE"):
        _fail("lv.label missing LONG_MODE enum namespace")
    if hasattr(lv, "LABEL_LONG_MODE"):
        _fail("lv.LABEL_LONG_MODE must not be exposed at module level")
    print("OK: enum namespaces (lv.EVENT, lv.obj.FLAG, lv.label.LONG_MODE)")


def test_module_types(lv):
    for name in ("C_Pointer", "Blob", "Struct", "LvReferenceError"):
        if not hasattr(lv, name):
            _fail(f"missing module export lv.{name}")
    print("OK: module types (C_Pointer, Blob, Struct, LvReferenceError)")


def test_struct_helpers(lv):
    size = lv.color_t.__SIZE__
    if not isinstance(size, int) or size <= 0:
        _fail("lv.color_t.__SIZE__ missing or invalid")
    for name in ("__cast__", "__dereference__", "__cast_instance__"):
        if not hasattr(lv.color_t, name):
            _fail(f"lv.color_t missing helper {name}")
    print("OK: struct helpers (__SIZE__, __cast__, …)")


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


def test_multi_callbacks(lv):
    """Multiple add_event_cb registrations on one object must not clobber each other."""
    scr = lv.screen_active()
    btn = lv.button(scr)
    btn.set_size(80, 40)
    fired = []

    def mk(name):
        def cb(event):
            fired.append((name, event.get_code()))

        return cb

    for name, code in (
        ("PRESSED", lv.EVENT.PRESSED),
        ("RELEASED", lv.EVENT.RELEASED),
        ("CLICKED", lv.EVENT.CLICKED),
    ):
        btn.add_event_cb(mk(name), code, None)

    btn.send_event(lv.EVENT.PRESSED, None)
    btn.send_event(lv.EVENT.CLICKED, None)
    btn.send_event(lv.EVENT.RELEASED, None)

    expected = [
        ("PRESSED", lv.EVENT.PRESSED),
        ("CLICKED", lv.EVENT.CLICKED),
        ("RELEASED", lv.EVENT.RELEASED),
    ]
    if fired != expected:
        _fail(f"multi-callback dispatch mismatch: got {fired!r}, expected {expected!r}")
    print("OK: multiple filtered callbacks on one object")


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


def test_remove_style_none(lv):
    """remove_style(None, part) must pass NULL to LVGL, not dereference Py_None."""
    scr = lv.screen_active()
    arc = lv.arc(scr)
    arc.set_size(40, 40)
    arc.remove_style(None, lv.PART.KNOB)
    print("OK: arc.remove_style(None, PART.KNOB)")


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
    test_module_types(lv)
    test_struct_helpers(lv)
    test_widget_types(lv)
    test_module_functions(lv)
    test_nesting(lv)

    lv.init()
    try:
        _setup_display(lv)
        test_refr_now(lv)
        test_blob_dereference(lv)
        test_widget(lv)
        test_remove_style_none(lv)
        test_callbacks(lv)
        test_multi_callbacks(lv)
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
