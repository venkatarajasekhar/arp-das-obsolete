# This is the boilerplate doit script invoked by "." execution
# from doit scripts generated by mkdoit2

typeset winrunning=no _scrptname=$0
typeset Experiment HomeDir FlightNode wait_for_node
typeset gcpid _scr0 doit_not doit_stop _scrdefs

namewait -t0 qnx/screen 2>/dev/null && winrunning=yes
if [ $winrunning = yes ] && [ -x /windows/bin/Notice ]; then
  function nl_error {
	Notice -Eat "$_scrptname" $* >&2
	[ -n "$gcpid" ] && getcon -q $gcpid
	exit 1
  }
else
  function nl_error {
	echo "$_scrptname: $*" >&2
	[ -n "$gcpid" ] && getcon -q $gcpid
	exit 1
  }
fi

if [ $winrunning = yes ]; then
  [ ! -c /dev/win1 ] && /windows/bin/wterm true
  [ ! -c /dev/win1 ] && nl_error Unable to start /dev/win
fi

cd `dirname $0`
cfile=Experiment.config
[ ! -f "$cfile" ] && nl_error Cannot locate $cfile
. $cfile
[ -z "$Experiment" ] && nl_error Experiment undefined in $cfile
export Experiment
[ -n "FlightNode" ] && export FlightNode

while getopts "W" option; do
  case $option in
	W) _scrdefs="yes";;
	\?) echo; exit 1;;
	*) echo Unsupported option: -$option; exit 1;;
  esac
done
let sval=$OPTIND-1
shift $sval

for i in ; do
  case $i in
    not) doit_not=yes;;
    stop) doit_stop=yes;;
    wait) wait_for_node=yes;;
    *) : ;;
  esac
done

if [ -n "$doit_not" -o -n "$doit_stop" ] && [ $winrunning = yes ]; then
  eval `getcon //$NODE/dev/win _scr0`
  [ -z $_scr0 ] && nl_error Unable to allocate status window
  winsetsize $_scr0 8 45 $0
else _scr0=`tty`
fi

[ -n "$doit_not" ] && {
  echo Deterring Startup of Experiment $Experiment
  echo Waiting for pick_file
  pick_file /dev/null
  [ -n "$gcpid" ] && getcon -q $gcpid
  exit 0
} > $_scr0 2>&1

[ -n "$doit_stop" ] && {
  if [ -z "$FlightNode" ]; then
	FlightNode=`namewait -n0 -t0 -G parent 2>/dev/null`
	if [ -z "$FlightNode" ]; then
	  [ -n "$gcpid" ] && getcon -q $gcpid
	  nl_error Unable to locate flight node for experiment $Experiment
	fi
  fi
  echo Shutting down Experiment $Experiment on Node $FlightNode
  on -f $FlightNode /usr/local/bin/startdbr quit
  [ -n "$gcpid" ] && getcon -q $gcpid
  exit 0
} > $_scr0 2>&1

# start_rtg <script file>
function start_rtg {
  if [ $winrunning = yes ]; then
	namewait -t0 huarp/rtg 2>/dev/null || {
	  #generate a real config file here!
	  [ ! -f $1 ] && {
		echo "PO RP \"\""
		echo "PC APC $1"
		echo "PA"
	  } > $1
	  on -t /dev/con1 /windows/apps/rtg/rtg -f $1
	}
  fi
}

_scr0=`tty`

typeset _msgopts _dcopts _cmdopts
