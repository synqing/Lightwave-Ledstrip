"""Allow running the package as ``python -m edgemixer_eval``."""

import sys

from .eval import main

sys.exit(main())
