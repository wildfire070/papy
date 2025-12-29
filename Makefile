# Makefile for Papyrix Reader firmware
# Wraps PlatformIO commands for convenience

.PHONY: all build build-release upload upload-release flash flash-release \
        clean format check monitor size erase build-fs upload-fs sleep-screen help

# Default target
all: help

# Build targets
build: ## Build firmware (default environment)
	pio run

build-release: ## Build release firmware
	pio run -e gh_release

# Upload targets
upload: ## Build and flash to device
	pio run --target upload

upload-release: ## Build and flash release firmware
	pio run -e gh_release --target upload

# Aliases
flash: upload ## Alias for upload

flash-release: upload-release ## Alias for upload-release

# Clean
clean: ## Clean build artifacts
	pio run --target clean

# Code quality
format: ## Format code with clang-format
	./bin/clang-format-fix

check: ## Run static analysis (cppcheck)
	pio check

# Device/debug
monitor: ## Open serial monitor
	pio device monitor

size: ## Show firmware size
	pio run --target size

erase: ## Erase device flash
	pio run --target erase

# Filesystem
build-fs: ## Build filesystem image
	pio run --target buildfs

upload-fs: ## Upload filesystem to device
	pio run --target uploadfs

# Image conversion
sleep-screen: ## Convert image to sleep screen BMP
ifdef INPUT
ifdef OUTPUT
	python3 scripts/create_sleep_screen_image.py $(INPUT) $(OUTPUT) $(ARGS)
else
	@echo "Usage: make sleep-screen INPUT=<image> OUTPUT=<bmp> [ARGS='--dither --bits 8']"
endif
else
	@echo "Usage: make sleep-screen INPUT=<image> OUTPUT=<bmp> [ARGS='--dither --bits 8']"
	@echo "Example: make sleep-screen INPUT=photo.jpg OUTPUT=sleep.bmp"
endif

## Help:

help: ## Show this help
	@echo "Papyrix Reader - Build System"
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@awk 'BEGIN {FS = ":.*##"; section=""} \
		/^##/ { section=substr($$0, 4); next } \
		/^[a-zA-Z_-]+:.*##/ { \
			if (section != "") { printf "\n\033[1m%s\033[0m\n", section; section="" } \
			printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 \
		}' $(MAKEFILE_LIST)
	@echo ""