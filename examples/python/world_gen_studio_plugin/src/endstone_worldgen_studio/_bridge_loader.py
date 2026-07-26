"""Import the native bridge bundled in the WorldGen platform wheel."""

from __future__ import annotations

import importlib
from types import ModuleType


BRIDGE_MODULE = "_endstone_worldgen_live"
BUNDLED_BRIDGE_MODULE = f"{__package__}.{BRIDGE_MODULE}"


def _missing_target(error: ModuleNotFoundError, target: str) -> bool:
    """Distinguish an absent bridge from an error inside a present bridge."""
    return error.name == target


def import_live_bridge(expected_version: str) -> ModuleType:
    """Load only the package-local bridge from the self-contained platform wheel."""
    try:
        return importlib.import_module(BUNDLED_BRIDGE_MODULE)
    except ModuleNotFoundError as error:
        if not _missing_target(error, BUNDLED_BRIDGE_MODULE):
            raise
        raise ModuleNotFoundError(
            "WorldGen's native bridge is not installed. Install the matching "
            f"{expected_version} CPython 3.14 platform wheel for this operating system; "
            "the portable py3-none-any command wheel does not contain the native bridge.",
            name=BUNDLED_BRIDGE_MODULE,
        ) from error
