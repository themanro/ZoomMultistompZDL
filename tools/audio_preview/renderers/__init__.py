"""Desktop reference renderers keyed by source-directory name."""

from .arrakis import render as render_arrakis
from .corrupt import render as render_corrupt
from .flower import render as render_flower
from .gain import render as render_gain
from .genloss import render as render_genloss
from .klang import render as render_klang
from .microloom import render as render_microloom
from .purestdrive import render as render_purestdrive
from .scorch import render as render_scorch
from .shatter import render as render_shatter
from .stereochorus import render as render_stereochorus
from .tapeecho4 import render as render_tapeecho4
from .tapehack import render as render_tapehack

RENDERERS = {
    "arrakis": render_arrakis,
    "corrupt": render_corrupt,
    "flower": render_flower,
    "gain": render_gain,
    "genloss": render_genloss,
    "klang": render_klang,
    "microloom": render_microloom,
    "purestdrive": render_purestdrive,
    "scorch": render_scorch,
    "shatter": render_shatter,
    "stereochorus": render_stereochorus,
    "tapeecho4": render_tapeecho4,
    "tapehack": render_tapehack,
}
