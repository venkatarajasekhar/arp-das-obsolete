# cycle language for translating *.awk to *.tmc
# $Id$
# Possible changes:
#   Consider changing regions to not use states, just conditions.
#   Region A ( Cycle_time > 2 && Cycle_time < 5 && (HCtStat & 0x8000)==0)
#	This would allow non-connected regions and incorporate the 
#	discard feature. On the other hand, discard is variable-
#	specific. Hmmm.

function output_trigger() {
  if (Trigger_Output == 1) {
	printf "state (Trigger_%s_armed, Trigger_%s_seen);\n", Trigger_name, Trigger_name
	printf "validate Trigger_%s_armed;\n", Trigger_name
	printf "double Trigger_%s_time;\n\n", Trigger_name
	printf "depending on (Trigger_%s_armed)\n", Trigger_name
	printf "  if (%s) {\n", Start_Cond
	printf "	Trigger_%s_time = dtime();\n", Trigger_name
	printf "	validate Trigger_%s_seen;\n", Trigger_name
	print  "  }"

	if (Prestart_Cond == "") Prestart_Cond = "!(" Start_Cond ")"
	printf "depending on (Trigger_%s_seen)\n", Trigger_name
	printf "  if (%s) validate Trigger_%s_armed;\n\n", Prestart_Cond, Trigger_name
	Trigger_Output = 0
  }
}

BEGIN {
  errlevel = 0
}

# /^[ \t]*[;#%]/ { next }
/^[ \t]*[Rr]ate/ {
  sub("^[ \t]*[Rr]ate[ \t]*", "")
  Rate = $0
  next
}
/^[ \t]*[Cc]onvert_on/ {
  convert_all = 1
  next
}
/^[ \t]*[Cc]onvert_off/ {
  convert_all = 0
  next
}
/^[ \t]*[Tt]rigger/ {
  Start_Cond = ""
  Prestart_Cond = ""
  T0 = ""
  T1 = ""
  Trigger_Output = 1
  Trigger_name = $2
  if (is_trigger[Trigger_name] == "yes") {
	system("echo cycle: Duplicate Trigger name " Trigger_name " >&2")
	errlevel = 1
	exit
  }
  is_trigger[Trigger_name] = yes
  Trigger[n_triggers++] = Trigger_name
  in_trigger = 0
  in_region = 0
  next
}
/^[ \t]*Start/ {
  sub("^[ \t]*[Ss]tart[ \t]*", "")
  Start_Cond = $0
  in_trigger = 1
  next
}
/^[ \t]*[Pp]restart/ {
  sub("^[ \t]*[Pp]restart[ \t]*", "")
  Prestart_Cond = $0
  next
}
/^[ \t]*[Dd]iscard .* unless/ {
  cond = $0
  sub( "^[ \t]*[Dd]iscard[ \t].*[ \t]unless[ \t]*", "", cond )
  for ( i=2; i < NF && $i != "unless"; i++ ) {
	discard[ $i ] = cond
  }
  next
}
# Region <name> <T0> <T1>
/^[ \t]*[Rr]egion/ {
  # Trigger Definition must be complete now, so output it.
  output_trigger()
  if ( Rate == "" ) {
	system("echo cycle: No Rate defined >&2")
	errlevel = 1
	exit
  }
  Region_name = $2
  T0 = $3
  T1 = $4
  i = Trigger_name "_" Region_name
  if (is_trigreg[i] == "yes") {
	system("echo cycle: NR  Trigger Region " i " already defined >&2")
	errlevel = 1
	exit
  } else is_trigreg[i] = yes
  if (is_region[Region_name] != "yes") {
	i = "Region_" Region_name
	printf "state (%s_inactive, ", i
	printf "%s_active, ", i
	printf "%s_complete);\n", i
	printf "validate %s_inactive;\n\n", i
	is_region[Region_name] = "yes"
  }
  printf "depending on (Trigger_%s_seen)", Trigger_name
  printf " validate Region_%s_inactive;\n", Region_name
  i = "TrigReg_" Trigger_name "_" Region_name
  printf "state (%s_inactive, %s_armed, %s_active);\n", i, i, i
  printf "validate %s_inactive;\n", i
  printf "depending on (Trigger_%s_seen)", Trigger_name
  printf " validate %s_armed;\n", i
  printf "depending on (%s_armed, %s) {\n", i, Rate
  printf "  if (dtime()-Trigger_%s_time >= %s) {\n", Trigger_name, T0
  printf "	depending on (Region_%s_active)", Region_name
  printf " validate %s_inactive;\n", i
  printf "	else validate %s_active;\n", i
  print  "  }"
  print  "}"
  printf "depending on (%s_active)", i
  printf " validate Region_%s_active;\n", Region_name
  printf "depending on (%s_active, %s) {\n", i, Rate
  printf "  if (dtime()-Trigger_%s_time > %s) {\n", Trigger_name, T1
  printf "	depending on (Region_%s_active)", Region_name
  printf " validate Region_%s_complete;\n", Region_name
  printf "	validate %s_inactive;\n", i
  print  "  }"
  print  "}"
  next
}
# Average <RegionName> <vars>
/^[ \t]*[Aa]verage/ {
  region_vars = 0
  Region_name = $2
  if (is_region[Region_name] != "yes") {
	system("echo cycle: " NR " Average region " Region_name " undefined >&2")
	errlevel = 1
	exit
  }
  if (region_averaged[Region_name] == "yes") {
	system("echo cycle: " NR " Region " Region_name " already averaged >&2")
	errlevel = 1
	exit
  }
  region_averaged[Region_name] = "yes"
  for (i = 3; i <= NF; i++) {
	vname = vsrc = $i;
	if ( match( vname, "^convert\\(" ) ) {
	  sub( "^convert\\(", "", vname )
	  sub( "\\)$", "_cvt", vname )
	} else if ( convert_all == 1 ) {
	  vsrc = "convert(" vsrc ")";
	}
	cond = discard[vname]
	vname = region_var[region_vars++] = vname "_" Region_name
	printf "\nint %s_c;\n", vname
	printf "double %s_s;\n", vname
	printf "double %s;\n", vname
	printf "invalidate %s;\n", vname
	printf "depending on (Region_%s_inactive once, %s)", Region_name, vname
	printf " invalidate %s;\n", vname
	printf "depending on (Region_%s_active once) {\n", Region_name
	printf "  %s_c = 0;\n", vname
	printf "  %s_s = 0;\n", vname
	print  "}"
	printf "depending on (Region_%s_active, %s) {\n", Region_name, Rate
	indent = ""
	if ( cond ) {
	  printf "  if (" cond ") {\n"
	  indent = "  "
	}
	printf "  %s%s_c++;\n", indent, vname
	printf "  %s%s_s += %s;\n", indent, vname, vsrc
	if ( indent != "" ) print "  }"
	print  "}"
  }
  printf "depending on (Region_%s_complete) {\n", Region_name
  for (i = 0; i < region_vars; i++) {
	vname = region_var[i];
	printf "  if (%s_c > 0) {\n", vname
	printf "	%s = %s_s/%s_c;\n", vname, vname, vname
	printf "	validate %s;\n", vname
	printf "  }\n"
  }
  print "}"
  next
}
/[^ \t]/ { print }
END {
  if ( errlevel == 1 ) exit 1
  output_trigger()
}
