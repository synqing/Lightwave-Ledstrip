"""Configuration management and metadata tracking for reproducible runs."""

import json
import subprocess
import platform
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Optional
from datetime import datetime


@dataclass
class GitMetadata:
    """Git repository metadata for reproducibility."""
    
    sha: str
    branch: str
    dirty: bool
    remote_url: Optional[str] = None
    
    @classmethod
    def capture(cls, repo_path: Optional[Path] = None) -> "GitMetadata":
        """Capture current git state."""
        if repo_path is None:
            repo_path = Path.cwd()
        
        try:
            # Get SHA
            sha = subprocess.check_output(
                ["git", "rev-parse", "HEAD"],
                cwd=repo_path,
                stderr=subprocess.DEVNULL
            ).decode().strip()
            
            # Get branch
            branch = subprocess.check_output(
                ["git", "rev-parse", "--abbrev-ref", "HEAD"],
                cwd=repo_path,
                stderr=subprocess.DEVNULL
            ).decode().strip()
            
            # Check if dirty
            status = subprocess.check_output(
                ["git", "status", "--porcelain"],
                cwd=repo_path,
                stderr=subprocess.DEVNULL
            ).decode().strip()
            dirty = len(status) > 0
            
            # Get remote URL
            try:
                remote_url = subprocess.check_output(
                    ["git", "config", "--get", "remote.origin.url"],
                    cwd=repo_path,
                    stderr=subprocess.DEVNULL
                ).decode().strip()
            except subprocess.CalledProcessError:
                remote_url = None
            
            return cls(
                sha=sha,
                branch=branch,
                dirty=dirty,
                remote_url=remote_url
            )
        except (subprocess.CalledProcessError, FileNotFoundError):
            return cls(
                sha="unknown",
                branch="unknown",
                dirty=True,
                remote_url=None
            )


@dataclass
class BenchConfig:
    """Complete configuration for a benchmark run."""
    
    # Test parameters
    num_leds: int = 160
    num_frames: int = 512
    seed: int = 123
    
    # Pipeline configurations
    enable_lwos_bayer: bool = True
    enable_lwos_temporal: bool = True
    enable_sb_quantiser: bool = True
    enable_emo_quantiser: bool = True
    
    # Stimulus settings
    stimuli: list[str] = None
    brightness_levels: list[float] = None
    gamma_values: list[float] = None
    
    # Output settings
    save_frames: bool = False
    save_plots: bool = True
    output_dir: Optional[Path] = None
    
    # System metadata
    timestamp: Optional[str] = None
    git: Optional[GitMetadata] = None
    python_version: Optional[str] = None
    platform_info: Optional[str] = None
    
    def __post_init__(self):
        """Set defaults and capture system metadata."""
        if self.stimuli is None:
            self.stimuli = ["ramp", "constant", "gradient", "palette_blend"]
        
        if self.brightness_levels is None:
            self.brightness_levels = [0.1, 0.5, 1.0]
        
        if self.gamma_values is None:
            self.gamma_values = [1.5, 2.2]
        
        if self.timestamp is None:
            self.timestamp = datetime.now().isoformat()
        
        if self.git is None:
            self.git = GitMetadata.capture()
        
        if self.python_version is None:
            import sys
            self.python_version = sys.version
        
        if self.platform_info is None:
            self.platform_info = platform.platform()
        
        if self.output_dir is not None:
            self.output_dir = Path(self.output_dir)
    
    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        d = asdict(self)
        # Convert Path to string
        if d['output_dir'] is not None:
            d['output_dir'] = str(d['output_dir'])
        return d
    
    def save(self, path: Path):
        """Save configuration to JSON file."""
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(path, 'w') as f:
            json.dump(self.to_dict(), f, indent=2)
    
    @classmethod
    def load(cls, path: Path) -> "BenchConfig":
        """Load configuration from JSON file."""
        with open(path, 'r') as f:
            data = json.load(f)
        
        # Reconstruct GitMetadata
        if 'git' in data and data['git'] is not None:
            data['git'] = GitMetadata(**data['git'])
        
        # Convert output_dir back to Path
        if 'output_dir' in data and data['output_dir'] is not None:
            data['output_dir'] = Path(data['output_dir'])
        
        return cls(**data)
