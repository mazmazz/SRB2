# Declare Emscripten (WebAssembly) sources

# Unimplemented features are commented out

set(EMCC_ARGS "SHELL:")
set(EMCC_LINK_ARGS "SHELL:")

########################################################################
# Configuration
########################################################################

set(SRB2_CONFIG_EMCC_OUTPUT_TYPE ".js" CACHE STRING
	"Output file type for target: .mjs, .js, .html, .a")

set(SRB2_CONFIG_EMCC_HAVE_ASYNCIFY OFF CACHE BOOL
	"Enable asyncify behavior.")

set(SRB2_CONFIG_EMCC_SOURCEMAP_URL_BASE "http://localhost:3000/" CACHE STRING
	"Base URL where the debug source map is located. Include trailing slash.")

set(SRB2_CONFIG_EMCC_SINGLE_FILE OFF CACHE STRING
	"Compile one output file that combines JS, WASM, and HTML.")

set(SRB2_CONFIG_EMCC_SHELL_FILE "" CACHE STRING
	"Path to shell HTML file. Optional.")

set(SRB2_CONFIG_EMCC_PRELOAD_PATH "" CACHE STRING
	"Path to directory or file to preload on Emscripten startup. If path is a directory, then all files in the directory will be preloaded.")

#if(${SRB2_CONFIG_HWRENDER})
	# set(SRB2_CONFIG_EMCC_WEBGL2 ON CACHE BOOL
	# 	"Enable support for WebGL 2.")
#endif()

########################################################################
# Definitions
########################################################################

# -DEMSCRIPTEN set in <repo_root>/CMakeLists.txt

# Opt-out features
set(EMCC_ARGS ${EMCC_ARGS}
	-DNONET=1
	-DNOHW=1
	-DNOASM=1

	# Set in src/CMakeLists.txt
	#-DNORUSEASM=1
	#-DNONX86=1
)

# Opt-in features
set(EMCC_ARGS ${EMCC_ARGS}
# -DLOWMEMORY=1
# -DFWAD=1
	-DTOUCHINPUTS
	-DMAINLOOPBYFUNCTION
)

if(${SRB2_CONFIG_EMCC_HAVE_ASYNCIFY})
	set(EMCC_ARGS ${EMCC_ARGS} -DHAVE_ASYNCIFY)
	set(EMCC_ARGS "${EMCC_ARGS} -s ASYNCIFY=1")
else()
	set(EMCC_ARGS ${EMCC_ARGS} -DNOWIPE)
endif()

########################################################################
# Source files
########################################################################

set(SRB2_EMSCRIPTEN_SOURCES
	i_emscripten.c
)

set(SRB2_EMSCRIPTEN_HEADERS
	i_emscripten.h
)

prepend_sources(SRB2_EMSCRIPTEN_SOURCES)
prepend_sources(SRB2_EMSCRIPTEN_HEADERS)

source_group("Emscripten" FILES ${SRB2_EMSCRIPTEN_SOURCES} ${SRB2_EMSCRIPTEN_HEADERS})

target_sources(SRB2SDL2 PRIVATE
	${SRB2_EMSCRIPTEN_SOURCES}
	${SRB2_EMSCRIPTEN_HEADERS}
)

########################################################################
# Libraries
########################################################################

set(EMCC_LIBRARIES "SHELL: \
	-s USE_LIBPNG=1 \
	-s USE_SDL=2 \
	-s USE_SDL_IMAGE=2 \
	-s USE_ZLIB=1"
)

target_compile_options(SRB2SDL2 PRIVATE ${EMCC_LIBRARIES})
target_link_options(SRB2SDL2 PRIVATE ${EMCC_LIBRARIES})

########################################################################
# Compiler setup
########################################################################

# Set build parameters
if (${CMAKE_BUILD_TYPE} MATCHES Debug)
	# Set Emscripten initial memory size
	set(EMCC_INITIAL_MEMORY "101187584")

	set(OPTIMIZATION_LEVEL "0")
	set(DEBUG_LEVEL "4")

	# if(${SRB2_CONFIG_HWRENDER})
	# 	set(WEBGL_DEBUG "-s GL_ASSERTIONS=1 ")
	# endif()

	# Prepare --source-map-base argument for use when debug is enabled
	if(NOT ("${SRB2_CONFIG_EMCC_SOURCEMAP_URL_BASE}" STREQUAL ""))
		set(EMCC_SOURCE_MAP_BASE "--source-map-base \"${SRB2_CONFIG_EMCC_SOURCEMAP_URL_BASE}\"")
	endif()
else()
	set(EMCC_INITIAL_MEMORY "99287040")
	set(OPTIMIZATION_LEVEL "3")
	set(DEBUG_LEVEL "0")
endif()

# Toggle single file output
if(SRB2_CONFIG_EMCC_SINGLE_FILE)
	set(EMCC_SINGLE_FILE "-s SINGLE_FILE=1")
endif()

# Import shell HTML
if(NOT ("${SRB2_CONFIG_EMCC_SHELL_FILE}" STREQUAL ""))
	if(EXISTS "${SRB2_CONFIG_EMCC_SHELL_FILE}")
		set(EMCC_SHELL "--shell-file \"${SRB2_CONFIG_EMCC_SHELL_FILE}\"")
	else()
		message(SEND_ERROR "Shell HTML file not found:\n"
			"       ${SRB2_CONFIG_EMCC_SHELL_FILE}")
	endif()
endif()

# Set preload path
if(NOT ("${SRB2_CONFIG_EMCC_PRELOAD_PATH}" STREQUAL ""))
	if(EXISTS "${SRB2_CONFIG_EMCC_PRELOAD_PATH}")
		# Dodge error about path below current directory
		file(COPY "${SRB2_CONFIG_EMCC_PRELOAD_PATH}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
		# Get base name
		get_filename_component(EMCC_PRELOAD_NAME "${SRB2_CONFIG_EMCC_PRELOAD_PATH}" NAME)
		# Prepare linker flag
		if(IS_DIRECTORY "${SRB2_CONFIG_EMCC_PRELOAD_PATH}")
			set(EMCC_PRELOAD "--preload-file \"${CMAKE_CURRENT_BINARY_DIR}/${EMCC_PRELOAD_NAME}\"@\"/\"")
		else()
			set(EMCC_PRELOAD "--preload-file \"${CMAKE_CURRENT_BINARY_DIR}/${EMCC_PRELOAD_NAME}\"@\"/${EMCC_PRELOAD_NAME}\"")
		endif()
	else()
		message(SEND_ERROR "Preload path not found:\n"
			"       ${EMCC_PRELOAD_PATH}")
	endif()
endif()

# Configure WebGL
# if(${SRB2_CONFIG_HWRENDER})
	# set(EMCC_ARGS "${EMCC_ARGS} -s FULL_ES2=1")
	# if(SRB2_CONFIG_EMCC_WEBGL2)
	# 	set(WEBGL2 "-s USE_WEBGL2=1 ")
	#	set(EMCC_ARGS "${EMCC_ARGS} -s FULL_ES3=1")
	# 	add_definitions(-DUSE_ES3)
	# else()
	# 	set(WEBGL2 "")
	# endif()
# endif()

# Export functions to JS.
set(EMCC_EXPORT_LIST "['ccall','_main','_Em_Program_Main','_Em_Program_Loop']")

# set(EMCC_EXPORT_LIST "['ccall','_main','_main_cont','_Emsh_FinalizeQuit','_Emsh_Halt',\
# '_Emsh_PauseLoop','_Emsh_ResumeLoop','_Em_UpdateFullscreenStatus','_Em_ChangeResolution',\
# '_Emsh_InjectKeycode','_Emsh_InjectText','_Emsh_LockMouse','_Emsh_UnlockMouse','_Emsh_AddFileInfo'")

# Set compiler args
set(EMCC_ARGS ${EMCC_ARGS}
	-O${OPTIMIZATION_LEVEL}
	--llvm-opts ${OPTIMIZATION_LEVEL}
	-g${DEBUG_LEVEL}
)

# Set linker args
set(EMCC_LINK_ARGS "${EMCC_LINK_ARGS} \
	${EMCC_SINGLE_FILE} \
	-s TOTAL_MEMORY=${EMCC_INITIAL_MEMORY} \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s \"EXTRA_EXPORTED_RUNTIME_METHODS=['ccall']\" \
	-s \"EXPORTED_FUNCTIONS=${EMCC_EXPORT_LIST}\" \

	${EMCC_SOURCE_MAP_BASE} \
	${EMCC_PRELOAD} \
	${EMCC_SHELL} \
	-lidbfs.js"

	# ${WEBGL2}
	# ${WEBGL_DEBUG}
)

# Wire it all together
set_target_properties(SRB2SDL2 PROPERTIES SUFFIX ${SRB2_CONFIG_EMCC_OUTPUT_TYPE})
target_compile_options(SRB2SDL2 PRIVATE ${EMCC_ARGS})
target_link_options(SRB2SDL2 PRIVATE ${EMCC_ARGS})
target_link_options(SRB2SDL2 PRIVATE ${EMCC_LINK_ARGS})

########################################################################
# Installation
########################################################################

get_target_property(SRB2_OUTPUT_NAME SRB2SDL2 OUTPUT_NAME)

install(DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
	DESTINATION "${CMAKE_INSTALL_PREFIX}"
	FILES_MATCHING
		PATTERN "${SRB2_OUTPUT_NAME}.data"
		PATTERN "${SRB2_OUTPUT_NAME}.html"
		PATTERN "${SRB2_OUTPUT_NAME}.js"
		PATTERN "${SRB2_OUTPUT_NAME}.mjs"
		PATTERN "${SRB2_OUTPUT_NAME}.wasm"
)
