#!/bin/bash

unset ENV_CHIP
unset ENV_CORE
unset ENV_OS
unset ENV_TOOLCHAIN_FILE
unset ENV_ANDROID_TOOLCHAIN_FILE
unset ENV_SELECTED_CODECS
unset ENV_BUILD_AARCH32
unset ENV_BUILD_AARCH64
unset ENV_NDK_VER

TOP=${PWD}

# Clean and create build directory
cd ${TOP}
rm -fr tmp
mkdir tmp
cd tmp

# Get the number of CPU cores
num_cores=$(nproc)

# Set the number of parallel jobs to the number of CPU cores
make_jobs="-j${num_cores}"

function handle_error() {
	local error_message=$1
	local script_name=$(basename "${BASH_SOURCE[0]}")  # Get the script filename without path
	err "${script_name}: $error_message"
	cd ${TOP}
	#exit 1
	return 1  # This will exit the function, but not the shell session.
}

function info() {
	echo -ne "\033[32m"
	echo "$@"
	echo -ne "\033[0m"
}

function log() {
	echo -ne "\033[33m"
	echo "$@"
	echo -ne "\033[0m"
}

function warn() {
	echo -ne "\033[35m"
	echo "$@"
	echo -ne "\033[0m"
}

function err() {
	echo -ne "\033[31m"
	echo "$@"
	echo -ne "\033[0m"
}

function cmd() {
	echo -ne "\033[36m"
	echo "$@"
	echo -ne "\033[0m"
	eval "$@"
	if [ $? -ne 0 ]; then
		handle_error "Command failed: $@"
		return 1
	fi
	echo -ne "\033[0m"
}

function print_chipset_info_help() {
	local info_file="${TOP}/config/tcc_chipset_info.txt"
	if [ -f "$info_file" ]; then
		while IFS= read -r line; do
			echo -e "$line"
		done < "$info_file"
	else
		echo -e "\033[31mError: $info_file not found!\033[0m"
	fi
}

function print_ip_info_help() {
	local info_file="${TOP}/config/tcc_video_ip_info.txt"
	if [ -f "$info_file" ]; then
		while IFS= read -r line; do
			echo -e "$line"
		done < "$info_file"
	else
		echo -e "\033[31mError: $info_file not found!\033[0m"
	fi
}

##########################################################################
# Menu
# Clear this variable. It will be built up again when the chipsetsetup.sh files are included.
unset CHIPSET_CHOICES
unset OS_CHOICES

function add_chipset_menu() {
	local new_menu="$1"
	local c
	for c in "${CHIPSET_CHOICES[@]}" ; do
		if [ "$new_menu" = "$c" ] ; then
			return
		fi
	done
	CHIPSET_CHOICES+=("$new_menu")
}

function add_os_menu() {
	local new_menu=$1
	local c
	for c in ${OS_CHOICES[@]} ; do
		if [ "$new_menu" = "$c" ] ; then
			return
		fi
	done
	OS_CHOICES=(${OS_CHOICES[@]} "$new_menu")
}

# Add chipset combos
add_chipset_menu "TCC750x-A53"
add_chipset_menu "TCC805x-A72"
add_chipset_menu "TCC805x-A53-Sub Cluster"


# Add OS combos
add_os_menu "Linux"
add_os_menu "Android"

function print_chipset_menu() {
	echo
	warn "Select a chipset and core from the options below:"
	log "Chipset Menu - Choose a combination:"
	local i=1
	local choice
	for choice in "${CHIPSET_CHOICES[@]}"
	do
		printf " %2d) %s\n" $i "$choice"
		i=$(($i+1))
	done
	echo
}

function print_os_menu() {
	echo
	warn "Select an OS from the options below:"
	log "OS Menu - Choose an OS:"

	local i=1
	local choice
	for choice in ${OS_CHOICES[@]}
	do
		printf " %2d) %s\n" $i "$choice"
		i=$(($i+1))
	done
	echo
}

function select_chipset() {
	while true; do
		print_chipset_menu
		read -p "Enter your choice: " choice

		if ! [[ "$choice" =~ ^[0-9]+$ ]] || [ "$choice" -lt 1 ] || [ "$choice" -gt ${#CHIPSET_CHOICES[@]} ]; then
			handle_error "Invalid choice. Exiting."
			return 1
		fi

		local selected_chipset=${CHIPSET_CHOICES[$(($choice-1))]}
		info "Selected chipset: $selected_chipset"

		# Extract the CHIP and CORE from the selected choice
		ENV_CHIP=$(echo $selected_chipset | cut -d'-' -f1)
		ENV_CORE=$(echo $selected_chipset | cut -d'-' -f2)
		break
	done
}

function select_os() {
	print_os_menu
	read -p "Enter your choice (default: All): " choice

	if [ -z "$choice" ]; then
		info "No OS selected, defaulting to build for all OS options (Linux, Android)."
		ENV_OS="All"
	else
		if ! [[ "$choice" =~ ^[0-9]+$ ]] || [ "$choice" -lt 1 ] || [ "$choice" -gt ${#OS_CHOICES[@]} ]; then
			handle_error "Invalid choice. Exiting."
			return 1
		fi
		local selected_os=${OS_CHOICES[$(($choice-1))]}
		info "Selected OS: $selected_os"
		ENV_OS=$selected_os
	fi
}

# CODEC CHOICES
CODEC_CHOICES=(
	"VPU4K_D2",
	"HEVC_E3",
	"VPU_C7"
)

function select_codecs() {
	echo
	warn "Select codecs to build from the options below (or default: Press Enter to build all):"
	log "Codec Menu - Choose one or more combinations (e.g., 1 2 3): "

	local i=1
	local choice
	for choice in "${CODEC_CHOICES[@]}"
	do
		printf " %2d) %s\n" $i "$choice"
		i=$(($i+1))
	done
	echo

	read -p "Enter your choices (default: All): " user_choices

	if [ -z "$user_choices" ]; then
		info "Selected all codecs."
		ENV_SELECTED_CODECS=("${CODEC_CHOICES[@]}")
	else
		ENV_SELECTED_CODECS=()
		for choice in $user_choices; do
			if ! [[ "$choice" =~ ^[0-9]+$ ]] || [[ "$choice" -lt 1 ]] || [[ "$choice" -gt ${#CODEC_CHOICES[@]} ]]; then
				handle_error "Invalid choice. Exiting."
				return 1
			fi
			ENV_SELECTED_CODECS+=("${CODEC_CHOICES[$(($choice-1))]}")
		done
		info "Selected codecs: ${ENV_SELECTED_CODECS[@]}"
	fi
	echo

	# Convert selected codecs into a comma-separated string
	ENV_SELECTED_CODECS=$(IFS=,; echo "${ENV_SELECTED_CODECS[*]}")
}

# Use the functions to select the chipset, OS, and codecs with error handling
select_chipset || return 1
select_os || return 1
select_codecs || return 1

##########################################################################
# Build Process
# Configure build options based on chipset and core
function configure_build_options() {
	if [[ "$ENV_CHIP" == "TCC750x" ]]; then
		ENV_BUILD_AARCH32=1
		ENV_BUILD_AARCH64=1
	elif [[ "$ENV_CHIP" == "TCC805x" ]]; then
		if [[ "$ENV_CORE" == "A72" ]]; then
			ENV_BUILD_AARCH32=1
			ENV_BUILD_AARCH64=1
		elif [[ "$ENV_CORE" == "A53" ]]; then
			ENV_BUILD_AARCH32=1
			ENV_BUILD_AARCH64=1
		fi
	else
		handle_error "Unsupported CHIP or CORE"
		return 1
	fi
}

# Main build process
configure_build_options
if [ $? -ne 0 ]; then
	return 1
fi

# build function
build() {
	local arch=$1
	local os=$2
	local toolchain_file=$3
	local build_type="Release"
#    local build_type="Debug"

	if [[ "$os" == "Android" ]]; then
		ENV_NDK_VER=r23c
		cmd "source ../config/${toolchain_file}"
		cmd "cmake \\
			-DCHIP=${ENV_CHIP} -DCORE=${ENV_CORE} \\
			-DCMAKE_TOOLCHAIN_FILE=${ENV_ANDROID_TOOLCHAIN_FILE} \\
			-DOS=${os} \\
			-DCMAKE_BUILD_TYPE=${build_type} \\
			-DANDROID_ABI=${ENV_ANDROID_ABI} \\
			-DANDROID_PLATFORM=${ENV_ANDROID_PLATFORM} \\
			-DANDROID_NDK=${ENV_ANDROID_NDK} \\
			-DSELECTED_CODECS=${ENV_SELECTED_CODECS} \\
			.."
	else
		cmd "cmake \\
			-DCHIP=${ENV_CHIP} -DCORE=${ENV_CORE} \\
			-DCMAKE_TOOLCHAIN_FILE=../config/${toolchain_file} \\
			-DOS=${os} \\
			-DCMAKE_BUILD_TYPE=${build_type} \\
			-DSELECTED_CODECS=${ENV_SELECTED_CODECS} \\
			.."
	fi

	if [ $? -ne 0 ]; then
		handle_error "${arch} CMake configuration failed."
		return 1
	fi

	# Make
	cmd "make clean ${make_jobs}"
	cmd "make ${make_jobs}"
	if [ $? -ne 0 ]; then
		handle_error "${arch} make failed."
		return 1
	fi

	# Clean and create build directory
	cd ${TOP}
	rm -fr tmp
	mkdir tmp
	cd tmp
}

build_start_time=$(date +"%s")

# Set the OS list based on the selected OS
if [[ "$ENV_OS" == "All" ]]; then
	ENV_OS_LIST=("Linux" "Android")
else
	ENV_OS_LIST=("$ENV_OS")
fi

# Build for each selected OS
for os in "${ENV_OS_LIST[@]}"; do
	if [[ "$os" == "Linux" ]]; then
		# Build 32-bit for Linux
		if [[ "$ENV_BUILD_AARCH32" == "1" ]]; then
			ENV_TOOLCHAIN_FILE="toolchain.arm.linux.config"
			echo "========================================================="
			warn "Building 32-bit for $os..."
			echo "========================================================="
			build "32-bit" "Linux" "$ENV_TOOLCHAIN_FILE"
			if [ $? -ne 0 ]; then return 1; fi
		fi

		# Build 64-bit for Linux
		if [[ "$ENV_BUILD_AARCH64" == "1" ]]; then
			ENV_TOOLCHAIN_FILE="toolchain.arm64.linux.config"
			echo "========================================================="
			warn "Building 64-bit for $os..."
			echo "========================================================="
			build "64-bit" "Linux" "$ENV_TOOLCHAIN_FILE"
			if [ $? -ne 0 ]; then return 1; fi
		fi
	fi

	if [[ "$os" == "Android" ]]; then
		# Build 32-bit for Android
		if [[ "$ENV_BUILD_AARCH32" == "1" ]]; then
			ENV_TOOLCHAIN_FILE="toolchain.arm.android.config"
			echo "========================================================="
			warn "Building 32-bit for $os..."
			echo "========================================================="
			build "32-bit" "Android" "$ENV_TOOLCHAIN_FILE"
			if [ $? -ne 0 ]; then return 1; fi
		fi

		# Build 64-bit for Android
		if [[ "$ENV_BUILD_AARCH64" == "1" ]]; then
			ENV_TOOLCHAIN_FILE="toolchain.arm64.android.config"
			echo "========================================================="
			warn "Building 64-bit for $os..."
			echo "========================================================="
			build "64-bit" "Android" "$ENV_TOOLCHAIN_FILE"
			if [ $? -ne 0 ]; then return 1; fi
		fi
	fi
done

##########################################################################
# Output the result of the build
cd ${TOP}/bin

echo "========================================================="
# Function to find and log output files for a specific OS
function find_and_log_files() {
	local os=$1
	local uppercase_os=$(echo "$os" | tr '[:lower:]' '[:upper:]')

	found_files=$(find . -type f -name "*${ENV_CHIP}*${uppercase_os}*.a" -newermt "@$build_start_time" 2>/dev/null | sed 's|^\./||')

	if [[ -n "$found_files" ]]; then
		warn "Build successful for $os:"
		while IFS= read -r file; do
			log "  - $file"
		done <<< "$found_files"
	else
		warn "No build artifacts found for $os."
	fi
}

# Search and log files for each selected OS
if [[ "$ENV_OS" == "All" || "$ENV_OS" == "Linux" ]]; then
	find_and_log_files "Linux"
fi

if [[ "$ENV_OS" == "All" || "$ENV_OS" == "Android" ]]; then
	find_and_log_files "Android"
fi

echo "========================================================="

##########################################################################
cd ${TOP}
rm -fr tmp
