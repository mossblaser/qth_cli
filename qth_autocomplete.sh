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
	CUR_WORD="${COMP_WORDS[COMP_CWORD]}"
	PRV_WORD="${COMP_WORDS[COMP_CWORD-1]}"
	
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
					get) BEHAVIOUR="DIRECTORY|PROPERTY-1:N" ;;
					set) BEHAVIOUR="DIRECTORY|PROPERTY-N:1" ;;
					delete) BEHAVIOUR="DIRECTORY|PROPERTY-N:1" ;;
					watch) BEHAVIOUR="DIRECTORY|EVENT-1:N" ;;
					send) BEHAVIOUR="DIRECTORY|EVENT-N:1" ;;
				esac
			fi
			
			# If not typing an option, try completing with topic names
			SUBDIR="$(_qth_to_dirname "$CUR_WORD")"
			"$CMD" ls --long "$SUBDIR" | sed -nre 's/^('"$BEHAVIOUR"')\t(.*)$/\2/p' | while read PART; do
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
			done 2>&1 > /dev/null
		fi
	)"
	
	IFS="$(echo -en "\n\t")" COMPREPLY=( $(compgen -W "$COMPLETIONS" -- "$CUR_WORD") )
	return 0
}

complete -o nospace -F _qth ./qth
complete -o nospace -F _qth qth
