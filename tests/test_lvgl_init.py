# SPDX-License-Identifier: MIT
"""Unit tests for the native lvgl CPython extension."""

import unittest


class LvglInitTests(unittest.TestCase):
    def test_import_and_init_deinit(self):
        import lvgl as lv

        lv.init()
        try:
            self.assertTrue(hasattr(lv, "obj"))
            self.assertTrue(hasattr(lv, "label"))
            self.assertTrue(hasattr(lv, "display_create"))
        finally:
            lv.deinit()

    def test_label_on_active_screen(self):
        import lvgl as lv

        lv.init()
        try:
            disp = lv.display_create(64, 64)
            lv.display_set_color_format(disp, lv.COLOR_FORMAT.RGB565)
            buf = lv.draw_buf_create(64, 64, lv.COLOR_FORMAT.RGB565, 0)
            lv.display_set_draw_buffers(disp, buf, None)
            lv.display_set_render_mode(disp, lv.DISPLAY_RENDER_MODE.PARTIAL)
            scr = lv.screen_active()
            label = lv.label(scr)
            label.set_text("hi")
            self.assertEqual(label.get_text(), "hi")
        finally:
            lv.deinit()


if __name__ == "__main__":
    unittest.main()
