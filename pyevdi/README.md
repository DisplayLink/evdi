# PyEvdi

The following commands assume you are in the `pyevdi` directory.

## Installing PyEvdi

```bash
python3 -m venv .venv_evdi      # Create virtual environment
source .venv_evdi/bin/activate  # Activate virtual environment
pip install pybind11            # Install dependencies
make -j$(nproc)                 # Build PyEvdi
make install                    # Install PyEvdi
```

## For Developers

### Testing PyEvdi

To run tests:
```bash
pip install pytest pytest-mock  # Install test dependencies
pytest test                     # Run tests
```

### Generate `compile_commands.json`

```bash
yay -Sy --needed bear   # Use your package manager to install bear
bear -- make            # Generate a `compile_commands.json` for autocompletion
```
