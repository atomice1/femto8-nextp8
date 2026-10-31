#!/usr/bin/env python3
"""
PICO-8 Cart Fuzzer

A fuzzing tool for PICO-8 carts that:
1. Generates random carts using generate_pico8_cart.py
2. Runs femto8 with each cart (with timeout)
3. Detects crashes (exit with signal) and hangs (timeout with flag set)
4. Saves failing carts to appropriate output directories

Usage:
    ./fuzz_pico8.py [options] <output_directory>

Options:
    --femto8 PATH       Path to femto8 binary (default: <script_dir>/../build-linux/femto8)
    --timeout SECONDS   Timeout in seconds (default: 5)
    --timeout-as-hang   Treat timeouts as hangs (default: no)
    --max-carts N       Maximum number of carts to generate (default: unlimited)
    --seed SEED         Initial random seed for reproducibility
    --quiet             Don't print progress messages
    --calls-only        Generate only function calls with random args

The output directory must exist and should be empty or contain previous fuzz results.
Generated carts that crash or hang are saved to:
    <output>/crashes/   - carts that caused a crash (exit with signal)
    <output>/hangs/     - carts that timed out (if --timeout-as-hang is set)
    <output>/temp/      - temporary directory for cart files

Example:
    ./fuzz_pico8.py --femto8 ./femto8 --timeout 3 --timeout-as-hang --max-carts 1000 ./fuzz_output
"""

import argparse
import os
import random
import signal
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional

# Import the cart generator as a module
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generate_pico8_cart


# AFL-style progress display
def clear_screen():
    """Clear the terminal screen."""
    print("\033[2J\033[H", end="")


def cursor_up(n=1):
    """Move cursor up n lines."""
    print(f"\033[{n}A", end="")


def cursor_bottom():
    """Move cursor to bottom of screen."""
    print("\033[B", end="")


def format_fuzz_stats(total, crashes, hangs, syntax_errors, successful, execs_per_sec, time_elapsed):
    """Format the fuzzing stats display similar to AFL."""
    total_pct = (crashes + hangs + syntax_errors + successful) / max(total, 1) * 100

    status_line = "== Fuzzer Status =="

    stats = f"""
{status_line:-^60}
[+] Carts executed   : {total}
[+] Crashes saved    : {crashes}
[+] Timeouts saved   : {hangs}
[+] Syntax errors    : {syntax_errors}
[+] Successes        : {successful}
[+] Execs per sec    : {execs_per_sec:.2f}
[+] Total time       : {time_elapsed:.1f}s
{'=':-^60}
"""
    return stats


class Fuzzer:
    """Main fuzzer class."""

    def __init__(self, femto8_path: str, timeout: float, timeout_as_hang: bool):
        self.femto8_path = femto8_path
        self.timeout = timeout
        self.timeout_as_hang = timeout_as_hang
        self.total_carts = 0
        self.crashes = 0
        self.hangs = 0
        self.syntax_errors = 0
        self.successful = 0
        self.start_time = time.time()
        self.last_display_time = 0
        self.last_total = 0

    def run_femto8(self, cart_path: str) -> tuple[int, str, float]:
        """
        Run femto8 with the given cart file.

        Returns:
            tuple(exit_code, output, elapsed_time)
            exit_code: 0 for success, positive for exit code, negative for signal
            output: stderr output from femto8
        """
        start_time = time.time()

        # Set SDL environment variables
        env = os.environ.copy()
        env["SDL_VIDEODRIVER"] = "dummy"
        env["SDL_AUDIODRIVER"] = "dummy"
        env["SDL_INPUTDRIVER"] = "dummy"

        try:
            # Run femto8 with timeout, capture stderr
            result = subprocess.run(
                [self.femto8_path, cart_path],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                timeout=self.timeout,
                env=env,
                text=True
            )
            elapsed = time.time() - start_time
            return result.returncode, result.stderr or "", elapsed

        except subprocess.TimeoutExpired:
            elapsed = time.time() - start_time
            # Return a negative exit code to indicate timeout
            return -signal.SIGALRM, "", elapsed

        except OSError as e:
            elapsed = time.time() - start_time
            # Return negative exit code for OS errors
            return -abs(e.errno if hasattr(e, 'errno') else 1), "", elapsed

    def update_stats(self, exit_code: int, output: str, elapsed: float, cart_path: str, output_dir: Path):
        """Update statistics and save crash/hang carts if needed."""
        self.total_carts += 1

        # Check for syntax error
        is_syntax_error = "syntax error" in output.lower()

        if exit_code == 0:
            # Success - no crash, no hang
            self.successful += 1
        elif is_syntax_error:
            # Syntax error - save to syntax_errors directory
            self.syntax_errors += 1
            cart_filename = Path(cart_path).name
            shutil.move(str(cart_path), output_dir / "syntax_errors" / cart_filename)
        elif exit_code < 0:
            # Signal or timeout
            if exit_code == -signal.SIGALRM:
                # Timeout
                if self.timeout_as_hang:
                    self.hangs += 1
                    # Save to hangs directory
                    cart_filename = Path(cart_path).name
                    shutil.move(str(cart_path), output_dir / "hangs" / cart_filename)
                else:
                    self.successful += 1  # Timeout but not treated as hang
            else:
                # Crash (signal)
                self.crashes += 1
                # Save to crashes directory
                cart_filename = Path(cart_path).name
                shutil.move(str(cart_path), output_dir / "crashes" / cart_filename)
        else:
            # Non-zero exit code (not a signal)
            self.successful += 1  # Not a crash/hang/syntax error

    def display_progress(self):
        """Display the fuzzing progress."""
        current_time = time.time()
        time_elapsed = current_time - self.start_time
        execs_per_sec = self.total_carts / time_elapsed if time_elapsed > 0 else 0

        # Clear and redraw the screen
        clear_screen()
        print(format_fuzz_stats(
            self.total_carts,
            self.crashes,
            self.hangs,
            self.syntax_errors,
            self.successful,
            execs_per_sec,
            time_elapsed
        ))

        self.last_display_time = current_time

    def fuzz(self, output_dir: Path, max_carts: Optional[int] = None,
             max_statements: int = 50, max_depth: int = 4,
             rng: Optional[random.Random] = None, quiet: bool = False,
             calls_only: bool = False):
        """
        Run the fuzzer.

        Args:
            output_dir: Output directory for results
            max_carts: Maximum number of carts to generate (None for unlimited)
            max_statements: Maximum Lua statements per cart
            max_depth: Maximum expression depth
            rng: Random number generator
            quiet: Don't print progress messages
            calls_only: If True, generate only function calls as statements
        """
        if rng is None:
            rng = random.Random()

        temp_dir = output_dir / "temp"
        temp_dir.mkdir(exist_ok=True)

        cart_num = 0
        try:
            while True:
                # Check if we've reached max carts
                if max_carts and self.total_carts >= max_carts:
                    break

                cart_num += 1

                # Generate cart using the generator module
                cart_content = generate_pico8_cart.generate_cart(
                    rng=rng,
                    max_lua_statements=max_statements,
                    max_depth=max_depth,
                    lua_only=False,
                    calls_only=calls_only,
                    seed=rng.randint(0, 2**32 - 1)  # Generate new seed for each cart
                )

                # Write cart to temp file
                cart_filename = f"cart_{cart_num:06d}_{int(rng.random() * 10000):04d}.p8"
                cart_path = temp_dir / cart_filename

                try:
                    with open(cart_path, 'w') as f:
                        f.write(cart_content)
                except IOError as e:
                    if not quiet:
                        print(f"Error writing cart file: {e}", file=sys.stderr)
                    continue

                # Run femto8
                exit_code, output, elapsed = self.run_femto8(str(cart_path))

                # Update stats and save if needed
                self.update_stats(exit_code, output, elapsed, str(cart_path), output_dir)

                # Remove cart file if not saved
                if cart_path.exists():
                    cart_path.unlink()

                # Update display
                if not quiet and self.total_carts % 10 == 0:
                    self.display_progress()

                # Small delay to avoid overwhelming the system
                time.sleep(0.01)

        except KeyboardInterrupt:
            if not quiet:
                print("\nInterrupted by user")

        finally:
            # Cleanup any remaining temp files
            for f in temp_dir.glob("cart_*.p8"):
                f.unlink()

            # Final display
            if not quiet:
                self.display_progress()
                print(f"\n=== Fuzzing Complete ===")
                print(f"Total carts: {self.total_carts}")
                print(f"Crashes: {self.crashes}")
                print(f"Hangs: {self.hangs}")
                print(f"Syntax errors: {self.syntax_errors}")
                print(f"Successful: {self.successful}")


def find_femto8(default_path: str) -> str:
    """Find the femto8 binary, or return the default path if not found."""
    if os.path.exists(default_path):
        return default_path

    # Try common alternatives
    alternatives = [
        os.path.join(os.path.dirname(default_path), "femto8"),
        "/home/chris/code/nextp8-dev/femto8-nextp8/build-linux/femto8",
        "/home/chris/code/nextp8-dev/femto8-nextp8/femto8",
    ]

    for alt in alternatives:
        if os.path.exists(alt):
            return alt

    # Return default if nothing found
    return default_path


def main():
    parser = argparse.ArgumentParser(
        description="Fuzz PICO-8 carts using random generation",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument("output_dir", help="Output directory for results")
    parser.add_argument("--femto8", "-f", default=None,
                       help="Path to femto8 binary (default: <script_dir>/../build-linux/femto8)")
    parser.add_argument("--timeout", "-t", type=float, default=5.0,
                       help="Timeout in seconds (default: 5)")
    parser.add_argument("--timeout-as-hang", action="store_true",
                       help="Treat timeouts as hangs")
    parser.add_argument("--max-carts", "-m", type=int, default=None,
                       help="Maximum number of carts to generate (default: unlimited)")
    parser.add_argument("--seed", "-s", type=int, default=None,
                       help="Initial random seed for reproducibility")
    parser.add_argument("--quiet", "-q", action="store_true",
                       help="Don't print progress messages")
    parser.add_argument("--max-statements", type=int, default=50,
                       help="Maximum Lua statements per cart (default: 50)")
    parser.add_argument("--max-depth", type=int, default=4,
                       help="Maximum expression depth (default: 4)")
    parser.add_argument("--calls-only", action="store_true",
                       help="Generate only function calls with random args")

    args = parser.parse_args()

    # Import shutil for file operations
    global shutil
    import shutil

    # Validate output directory
    output_dir = Path(args.output_dir)
    if not output_dir.exists():
        print(f"Error: Output directory '{output_dir}' does not exist", file=sys.stderr)
        sys.exit(1)
    if not output_dir.is_dir():
        print(f"Error: '{output_dir}' is not a directory", file=sys.stderr)
        sys.exit(1)

    # Setup output subdirectories
    crashes_dir = output_dir / "crashes"
    hangs_dir = output_dir / "hangs"
    syntax_errors_dir = output_dir / "syntax_errors"
    temp_dir = output_dir / "temp"

    for d in [crashes_dir, hangs_dir, syntax_errors_dir, temp_dir]:
        d.mkdir(exist_ok=True)

    # Find femto8 binary
    script_dir = Path(__file__).parent
    default_femto8 = script_dir / "../build-linux/femto8"
    femto8_path = args.femto8 if args.femto8 else str(default_femto8)
    femto8_path = find_femto8(femto8_path)

    if not os.path.exists(femto8_path):
        print(f"Error: femto8 binary not found at '{femto8_path}'", file=sys.stderr)
        print("Specify the correct path with --femto8", file=sys.stderr)
        sys.exit(1)

    # Initialize random generator
    rng = random.Random(args.seed)

    # Create and run fuzzer
    fuzzer = Fuzzer(femto8_path, args.timeout, args.timeout_as_hang)
    fuzzer.fuzz(
        output_dir=output_dir,
        max_carts=args.max_carts,
        max_statements=args.max_statements,
        max_depth=args.max_depth,
        rng=rng,
        quiet=args.quiet,
        calls_only=args.calls_only
    )


if __name__ == "__main__":
    main()
