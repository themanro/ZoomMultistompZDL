"""Desktop reference renderers keyed by source-directory name."""

from .tapeecho4 import render as render_tapeecho4

RENDERERS = {
    "tapeecho4": render_tapeecho4,
}
