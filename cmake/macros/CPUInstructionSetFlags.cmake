# Determine CPU architecture.
IF(MSVC AND _MSVC_C_ARCHITECTURE_FAMILY)
	# Check the MSVC architecture.
	# Set CMAKE_SYSTEM_PROCESSOR to match, since it doesn't get
	# set to the target architecture correctly.
	# TODO: Verify 32-bit.
	IF(_MSVC_C_ARCHITECTURE_FAMILY MATCHES "^[iI]?[xX3]86$")
		SET(CPU_i386 1)
		SET(CMAKE_SYSTEM_PROCESSOR "x86")
		SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "x86")
	ELSEIF(_MSVC_C_ARCHITECTURE_FAMILY MATCHES "^[xX]64$")
		SET(CPU_amd64 1)
		SET(CMAKE_SYSTEM_PROCESSOR "AMD64")
		SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "amd64")
	ELSEIF(_MSVC_C_ARCHITECTURE_FAMILY MATCHES "[iI][aA]64")
		SET(CPU_ia64 1)
		SET(CMAKE_SYSTEM_PROCESSOR "IA64")
		SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "ia64")
	ELSEIF(_MSVC_C_ARCHITECTURE_FAMILY STREQUAL "ARM" OR _MSVC_C_ARCHITECTURE_FAMILY STREQUAL "ARMV7")
		SET(CPU_arm 1)
		SET(CMAKE_SYSTEM_PROCESSOR "ARM")
		SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "arm")
	ELSEIF(_MSVC_C_ARCHITECTURE_FAMILY STREQUAL "ARM64EC")
		SET(CPU_arm64 1)
		SET(CPU_arm64ec 1)
		SET(CMAKE_SYSTEM_PROCESSOR "ARM64")
		# TODO: Does this change for ARM64EC?
		SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "arm64")
	ELSEIF(_MSVC_C_ARCHITECTURE_FAMILY STREQUAL "ARM64")
		SET(CPU_arm64 1)
		SET(CMAKE_SYSTEM_PROCESSOR "ARM64")
		SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "arm64")
	ELSE()
		MESSAGE(FATAL_ERROR "Unsupported value for _MSVC_C_ARCHITECTURE_FAMILY: ${_MSVC_C_ARCHITECTURE_FAMILY}")
	ENDIF()
ELSEIF(EMSCRIPTEN)
	# Emscripten: Use a custom CPU architecture.
	# zlib-ng uses "wasm32", so we'll use that too.
	SET(CPU_wasm32 1)
	SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "wasm32")
ELSE()
	# TODO: Verify cross-compile functionality.
	# TODO: ARM/ARM64 is untested.
	STRING(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" arch)
	IF(arch MATCHES "^(i.|x)86$|^x86_64$|^amd64$")
		IF(CMAKE_CL_64 OR ("${CMAKE_SIZEOF_VOID_P}" EQUAL 8))
			SET(CPU_amd64 1)
			SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "amd64")
		ELSE()
			SET(CPU_i386 1)
			SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "x86")
		ENDIF()
	ELSEIF(arch STREQUAL "ia64")
		SET(CPU_ia64 1)
		SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "ia64")
	ELSEIF(arch MATCHES "^arm(|v[1-7](.|h[fl]|hfe[lb]))?$" OR arch STREQUAL "aarch64" OR arch STREQUAL "arm64" OR arch STREQUAL "cortex")
		IF(CMAKE_CL_64 OR ("${CMAKE_SIZEOF_VOID_P}" EQUAL 8))
			SET(CPU_arm64 1)
			SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "arm64")
		ELSE()
			SET(CPU_arm 1)
			SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "arm")
		ENDIF()
	ELSEIF(arch MATCHES "^ppc" OR arch STREQUAL "powerpc")
		IF(arch STREQUAL "ppc64le")
			SET(CPU_ppc64le 1)
			SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "ppc64le")
		ELSEIF(CMAKE_CL_64 OR ("${CMAKE_SIZEOF_VOID_P}" EQUAL 8))
			SET(CPU_ppc64 1)
			SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "ppc64")
		ELSE()
			SET(CPU_ppc 1)
			SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "ppc")
		ENDIF()
	ELSEIF(arch MATCHES "^riscv")
		# TODO: Win32 manifest processor architecture, if it's ever ported to RISC-V.
		IF(CMAKE_CL_64 OR ("${CMAKE_SIZEOF_VOID_P}" EQUAL 8))
			SET(CPU_riscv64 1)
			SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "riscv64")
		ELSE()
			SET(CPU_riscv32 1)
			SET(WIN32_MANIFEST_PROCESSOR_ARCHITECTURE "riscv32")
		ENDIF()
	ELSE()
		MESSAGE(FATAL_ERROR "Unable to determine CPU architecture.\nCMAKE_SYSTEM_PROCESSOR == ${CMAKE_SYSTEM_PROCESSOR}")
	ENDIF()
	UNSET(arch)
ENDIF()

# i386/amd64: Flags for extended instruction sets.
IF(CPU_i386 OR CPU_amd64)
	# MSVC does not require anything past /arch:SSE2 for SSSE3.
	# ClangCL does require -mssse3, even on 64-bit.
	IF(MSVC)
		IF(CPU_i386)
			SET(SSE2_FLAG "/arch:SSE2")
			SET(SSSE3_FLAG "/arch:SSE2")
			SET(SSE41_FLAG "/arch:SSE2")
		ENDIF(CPU_i386)
		IF(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			SET(SSSE3_FLAG "-mssse3")
			SET(SSE41_FLAG "-msse4.1")
		ENDIF(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	ELSE()
		IF(CPU_i386)
			SET(MMX_FLAG "-mmmx")
			SET(SSE2_FLAG "-msse2")
		ENDIF(CPU_i386)
		SET(SSSE3_FLAG "-mssse3")
		SET(SSE41_FLAG "-msse4.1")
	ENDIF()
ENDIF(CPU_i386 OR CPU_amd64)

IF(CPU_arm OR CPU_arm64 OR CPU_arm64ec)
	# Check for arm_neon.h.
	# NOTE: Should always be present for arm64, but check anyway.
	INCLUDE(CheckIncludeFile)
	CHECK_INCLUDE_FILE("arm_neon.h" HAVE_ARM_NEON_H)
ENDIF(CPU_arm OR CPU_arm64 OR CPU_arm64ec)
