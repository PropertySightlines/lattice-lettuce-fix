# Facet Compositor — Build Targets
# Usage:
#   make test    — compile and run the rasterizer test suite
#   make demo    — alias for test (demo rendering is the test output)
#   make clean   — remove build artifacts

PROJECT_ROOT := $(shell git rev-parse --show-toplevel)
RUN_TEST     := $(PROJECT_ROOT)/scripts/run_test.sh

.PHONY: test demo clean window demo-window compositor demo-compositor ui demo-ui gpu demo-gpu demo-tiger rec-tiger video

test:
	@$(RUN_TEST) $(PROJECT_ROOT)/user/facet/raster/test_raster.salt

demo: test

window:
	@$(RUN_TEST) $(PROJECT_ROOT)/user/facet/window/test_window.salt

demo-window: window

compositor:
	@$(RUN_TEST) $(PROJECT_ROOT)/user/facet/compositor/test_compositor.salt

demo-compositor: compositor

ui:
	@$(RUN_TEST) $(PROJECT_ROOT)/user/facet/ui/test_ui.salt

demo-ui: ui

gpu:
	@$(RUN_TEST) $(PROJECT_ROOT)/user/facet/gpu/test_gpu.salt

demo-gpu: gpu

bench-raster-salt:
	@$(RUN_TEST) $(PROJECT_ROOT)/user/facet/raster/bench_raster.salt

bench-raster-c:
	@cd $(PROJECT_ROOT)/user/facet/raster && \
		clang -O3 -o /tmp/salt_build/bench_raster_c bench_raster_c.c facet_raster.c -lm && \
		/tmp/salt_build/bench_raster_c

bench-raster: bench-raster-c bench-raster-salt
	@echo ""
	@echo "Compare Golden checksums above — they must match."

demo-tiger:
	@$(RUN_TEST) $(PROJECT_ROOT)/user/facet/raster/demo_tiger.salt

rec-tiger:
	@mkdir -p /tmp/facet_frames
	@mkdir -p $(PROJECT_ROOT)/site/video
	@$(RUN_TEST) $(PROJECT_ROOT)/user/facet/raster/rec_tiger.salt
	@echo ""
	@echo "  Frames written to /tmp/facet_frames/"
	@echo "  Run 'make video' to encode."

video: rec-tiger
	@echo "  Encoding WebM..."
	@ffmpeg -y -framerate 5 -i /tmp/facet_frames/frame_%03d.ppm \
		-c:v libvpx-vp9 -b:v 0 -crf 30 -pix_fmt yuv420p \
		$(PROJECT_ROOT)/site/video/facet_tiger.webm
	@echo "  Encoding MP4..."
	@ffmpeg -y -framerate 5 -i /tmp/facet_frames/frame_%03d.ppm \
		-c:v libx264 -pix_fmt yuv420p -crf 20 \
		$(PROJECT_ROOT)/site/video/facet_tiger.mp4
	@echo "  ✓ site/video/facet_tiger.webm"
	@echo "  ✓ site/video/facet_tiger.mp4"

test-raster-c:
	@cd $(PROJECT_ROOT)/user/facet/raster && \
		clang -O2 -Wall -Wextra -o /tmp/salt_build/test_raster_c test_raster.c facet_raster.c -lm && \
		/tmp/salt_build/test_raster_c

clean:
	@rm -f /tmp/salt_build/test_raster /tmp/salt_build/test_raster.*
	@rm -f /tmp/salt_build/test_raster_c
	@rm -f /tmp/salt_build/test_window /tmp/salt_build/test_window.*
	@rm -f /tmp/salt_build/test_compositor /tmp/salt_build/test_compositor.*
	@rm -f /tmp/salt_build/test_gpu /tmp/salt_build/test_gpu.*
	@rm -f /tmp/salt_build/bench_raster /tmp/salt_build/bench_raster.*
	@rm -f /tmp/salt_build/bench_raster_c
	@rm -f /tmp/salt_build/rec_tiger /tmp/salt_build/rec_tiger.*
	@rm -rf /tmp/facet_frames
	@echo "  ✓ Clean"
