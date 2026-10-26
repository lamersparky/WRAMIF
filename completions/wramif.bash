# Bash completion for wramif
_wramif_complete() {
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    case "$prev" in
        --target|--output|--verify-script)
            compopt -o filenames 2>/dev/null || true
            mapfile -t COMPREPLY < <(compgen -f -- "$cur")
            return 0
            ;;
        --format)
            COMPREPLY=( $(compgen -W 'mp3 flac opus aac wav' -- "$cur") )
            return 0
            ;;
    esac
    opts="--target --output --format --recursive --no-recursive --no-duplicates --no-clipboard --windows-batch --batch-script --manual-input --resume --reenter-metadata --lyrics --artist --album --title --image --flush --verify-script --help"
    COMPREPLY=( $(compgen -W "$opts" -- "$cur") )
    return 0
}
complete -F _wramif_complete -o bashdefault -o default wramif
