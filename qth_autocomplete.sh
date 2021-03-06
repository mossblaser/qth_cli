################################################################################
# Autocomplete for the Qth commandline tool
################################################################################

# Filter the output of --help to just a list of subcommands
# Argument: the qth command name
_qth_help_to_subcommands() {
	CMD="$1"
	CLEAN_CMD="$(echo -n "$CMD" | tr -c "[[:alnum:]]" ".")"
	"$CMD" --help | sed -nre 's@^(usage| *or): '"$CLEAN_CMD"' ([a-zA-Z0-9]+) .*$@\2 @p'
}

# Filter the output of --help to just list all of the command line options
# Argument 1: the qth command name
# Argument 2: the qth subcommand used
_qth_help_to_options() {
	CMD="$1"
	SUB_CMD="$2"
	"$CMD" --help | awk '
		BEGIN {
			relevant = 0
		}
		/^optional arguments:$/ {
			relevant = 1
		}
		/^optional arguments when used with .*:$/ {
			relevant = 0
			for (i = 1; i <= NF; i++) {
				if ($i ~ /'"$SUB_CMD"',?/) {
					relevant = 1
				}
			}
		}
		/^  (-[a-zA-Z0-9]) ([a-zA-Z_]+ )?(--[-a-zA-Z0-9_]+)? .*/ {
			if (relevant) {
				for (i = 1; i <= NF; i++) {
					if ($i ~ /^--?[-a-zA-Z0-9]+$/) {
						print $i " "
					}
				}
			}
		}
	'
}

# Convert a topic to just the directory name
_qth_to_dirname() {
	echo -n "$1" | sed -nre 's@^(.*/)?[^/]*$@\1@p'
}

_qth() {
	CMD="${COMP_WORDS[0]}"
	SUB_CMD="$(echo -n "${COMP_WORDS[1]}" | grep -E '^[a-zA-Z0-9]+$')"
	
	# Truncate everything past the cursor and just get the pre-cursor part of the
	# current word.
	# XXX: This implementation is just made of awful hack since it doesn't seem
	# to be easy to get the cursor position within the current word...
	IFS=" " read -a COMP_WORDS_TRUNC <<< "${COMP_LINE:0:COMP_POINT}"
	if [ -n "${COMP_WORDS[COMP_CWORD]}" ]; then
		CUR_WORD="${COMP_WORDS_TRUNC[-1]}"
	else
		# Special case: the above fails since if the current word is empty the word
		# will be not appear in the array.
		CUR_WORD=""
	fi
	
	HELP_STRING="$("$CMD" --help)"
	
	COMPLETIONS="$(
		# Complete subcommands (if still on first argument)
		if [ "$COMP_CWORD" -eq 1 ]; then
			_qth_help_to_subcommands "$CMD"
		fi
		# Complete options (if started typing one)
		if [ "${CUR_WORD:0:1}" = "-" ]; then
			_qth_help_to_options "$CMD" "$SUB_CMD"
		else
			# Complete topics
			
			# Just suggest the right type of topics, unless -f --force is used
			BEHAVIOUR="[^\t]+"
			if echo "${COMP_WORDS[@]}" | grep -qvE '([-][^-f ]*f[^- ]*|--force)'; then
				case "$SUB_CMD" in
					get|set|delete) BEHAVIOUR="DIRECTORY|PROPERTY-1:N|PROPERTY-N:1" ;;
					watch|send) BEHAVIOUR="DIRECTORY|EVENT-1:N|EVENT-N:1" ;;
				esac
			fi
			
			# If not typing an option, try completing with topic names
			SUBDIR="$(_qth_to_dirname "$CUR_WORD")"
			("$CMD" ls --long "$SUBDIR" 2>/dev/null) | sed -nre 's/^('"$BEHAVIOUR"')\t(.*)$/\2/p' | while read PART; do
				LAST_CHAR="${PART:$((${#PART}-1)):1}"
				if [ "$LAST_CHAR" = "/" ]; then
					# List directories (for all types of commands)
					echo "$SUBDIR$PART"
				else
					# Only list non-directories when not 'ls'
					if [ "$SUB_CMD" != "ls" ]; then
						echo "$SUBDIR$PART "
					fi
				fi
			done
		fi
	)" 2>&1 > /dev/null
	
	IFS="$(echo -en "\n\t")" COMPREPLY=( $(compgen -W "$COMPLETIONS" -- "$CUR_WORD") )
	return 0
}

complete -o nospace -F _qth ./qth
complete -o nospace -F _qth qth
