/*
 * Mike Glover
 * mpg4@duluoz.net
 *
 * Johan Ekenberg
 * johan@ekenberg.se
 *
 * parse.c
 * command line parsing routines
 */

#include <config.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>


#include "quotatool.h"
#include "output.h"
#include "parse.h"
#include "quota.h"
#include "system.h"


#define WHITESPACE " \t\n"
#define ABC "abcdefghijklmnopqrstuvwxyzABCDEFGHIJLKMNOPQRSTUVWXYZ"

#if HAVE_GNU_GETOPT
#  define OPTSTRING "hVvnu::g::birq:l:t:dR"
#else
#  define OPTSTRING "hVvnu:g:birq:l:t:dR"
#endif


#define _PARSE_UNDEF 0x00
#define _PARSE_BLOCK 0x01
#define _PARSE_INODE 0x02
/*
 * parse_commandline
 * read our args, parse them
 * and return a struct of the data
 */
argdata_t *parse_commandline (int argc, char **argv)
{
  argdata_t *data;
  extern char *optarg;
  extern int optind, opterr, optopt;
  int done, fail;
  int quota_type;
  int opt;

  if (argc == 1) {
    output_help ();
    return NULL;
  }

  data = (argdata_t *) calloc (1,sizeof(argdata_t));
  if ( ! data ) {
    output_error ("Insufficient memory");
    exit (ERR_MEM);
  }

  quota_type = _PARSE_UNDEF;
  optarg = NULL;
  opterr = 0;
  done = fail = 0;
  while ( ! done && ! fail ) {
    opt = getopt(argc, argv, OPTSTRING);

    if (opt > 0)
       output_debug ("option: '%c', argument: '%s'", opt, optarg);

    switch (opt) {

    case EOF:
      done = 1;
      break;

    case 'h':
      output_help ();
      exit (0);

    case 'V':
      output_version();
      exit (0);

    case 'v':
      output_level++;
      break;

    case 'n':
      data->noaction = 1;
      break;


    case 'u':   /* set username */
      if ( data->id_type ) {
	output_error("Only one quota (user or group) can be set");
	fail = 1;
	continue;
      }
      data->id_type = QUOTA_USER;
#if HAVE_GNU_GETOPT
      /* -uuser */
      if ( optarg ) {
	output_debug ("not mangling: optarg='%s', next='%s'", optarg,
		      argv[optind]);
	data->id = optarg;
      }
      /* -u [-next-opt] */
      else if ( ! argv[optind] || argv[optind][0] == '-' ) {
	output_debug ("not mangling: NULL user");
	data->id = NULL;
      }
      /* -u user */
      else {
	output_debug ("mangling everything: next='%s'", argv[optind]);
	data->id = argv[optind];
	optind++;
      }
#else
      if (optarg && ((data->block_grace || data->inode_grace) || (optarg[0] == '-'))) {
          /* -u [-next-opt] */
          optind--;
          data->id = NULL;
      }
      else {
          /* -u user */
          data->id = optarg;
      }
#endif
      output_info ("using uid %s", data->id);
      break;

    case 'g':   /* set groupname */
      if ( data->id_type ) {
	output_error("Only one quota (user or group) can be set");
	fail = 1;
	continue;
      }
      data->id_type = QUOTA_GROUP;
#if HAVE_GNU_GETOPT
      if ( optarg ) {
	output_debug ("not mangling: optarg='%s', next='%s'", optarg,
		      argv[optind]);
	data->id = optarg;
      }
      else if ( ! argv[optind] || argv[optind][0] == '-' ) {
	output_debug ("not mangling: NULL user");
	data->id = NULL;
      }
      else {
	output_debug ("mangling everything: next='%s', argv[optind]");
	data->id = argv[optind];
	optind++;
      }
#else
      data->id = optarg;
#endif
      output_info ("using gid  %s", data->id);
      break;

    case 'b':   // Work with block limits
      output_info ("working with block limits");
      quota_type = _PARSE_BLOCK;
      break;

    case 'i':   // Work with inode limits
      output_info ("working with inode limits");
      quota_type = _PARSE_INODE;
      break;

    case 'q': // soft limit
      switch ( quota_type ) {
      case _PARSE_UNDEF:
	output_error ("Must specify either block (-b) or inode (-i) before -q");
	fail = 1;
	break;
      case _PARSE_BLOCK:
	data->block_soft = optarg;
	break;
      case _PARSE_INODE:
	data->inode_soft = optarg;
	break;
      default:
	output_error ("Impossible error #42q: evacuate the building!");
	break;
      }
      output_info ("setting soft limit to %s", optarg);
      break;


    case 'l': // hard limit
      switch ( quota_type ) {
      case _PARSE_UNDEF:
	output_error ("Must specify either block (-b) or inode (-i) before -l");
	fail = 1;
	break;
      case _PARSE_BLOCK:
	data->block_hard = optarg;
	break;
      case _PARSE_INODE:
	data->inode_hard = optarg;
	break;
      default:
	output_error ("Impossible error #42l: evacuate the building!");
	break;
      }
      output_info ("setting hard limit to %s", optarg);
      break;



    case 't': // set grace period
      data->id = NULL;
      switch ( quota_type ) {
      case _PARSE_UNDEF:
	output_error ("Must specify either block (-b) or inode (-i) before -t");
	fail = 1;
	break;
      case _PARSE_BLOCK:
	data->block_grace = optarg;
	break;
      case _PARSE_INODE:
	data->inode_grace = optarg;
	break;
      default:
	output_error ("Impossible error #42t: evacuate the building!");
	break;
      }
      break;


    case 'r': // reset grace time
      switch ( quota_type ) {
      case _PARSE_UNDEF:
	output_error ("Must specify either block (-b) or inode (-i) before -r");
	fail = 1;
	break;
      case _PARSE_BLOCK:
	data->block_reset = 1;
	break;
      case _PARSE_INODE:
	data->inode_reset = 1;
	break;
      default:
	output_error ("Impossible error #42r: evacuate the building!");
	break;
      }
      break;

    case 'd':
       data->dump_info = 1;
       break;

    case 'R':
       data->raise_only = 1;
       break;

    case ':':
      output_error ("Option '%c' requires an argument", optopt);
      break;

    case '?':
      output_error ("Unrecognized option: '%c'", optopt);
      // fall through

    default:
      output_help();
      fail = 1;
      break;
    }
  }

  if ( fail ) {
    free (data);
    return NULL;
  }

  if ( ! data->id_type ) {
    output_error ("Must specify either user or group quota");
    return NULL;
  }

  if ( data->dump_info) {
     output_info("Option 'd' => just dumping quota-info for %s", data->id_type == QUOTA_USER ? "user" : "group");
  }

  /* the remaining arg is the filesystem */
  data->qfile = argv[optind];
  if ( ! data->qfile || strlen(data->qfile) == 0) {
    output_error ("No filesystem specified");
    return NULL;
  }

  /* remove trailing slash(es) except for / filesystem */
  while (strlen(data->qfile) > 1) {
    if (data->qfile[strlen(data->qfile) - 1] != '/') break;
    data->qfile[strlen(data->qfile) - 1] = '\0';
  }

  /* check for mixing -t with other options in the wrong way */
  if (data->block_grace || data->inode_grace) {
     if (data->block_hard || data->block_soft || data->inode_hard || data->inode_soft || data->id) {
	output_error("Wrong options for -t, please see manpage for usage instructions!");
	return NULL;
     }
  }

  /* check for mixing -r with other options in the wrong way */
  if (data->block_reset || data->inode_reset) {
      if (data->block_hard || data->block_soft || data->inode_hard || data->inode_soft) {
          output_error("Wrong options for -r, please see manpage for usage instructions!");
          return NULL;
      }
  }

  output_info ("using filesystem %s", data->qfile);

  return data;
}

#define _PARSE_OP_ADD '+'
#define _PARSE_OP_SUB '-'


/* On aix MIN definition conflicts with MIN definition at
 * <include/sys/param.h>
 */
#undef MIN

#define SEC   1
#define MIN   60*SEC
#define HOUR  60*MIN
#define DAY   24*HOUR
#define WEEK  7*DAY
#define MONTH 30*DAY

/*
 * parse_timespan
 * understands seconds, minutes, hours, days, weeks, months
 * returns the number of seconds represented
 */
time_t parse_timespan (time_t orig, char *string)
{
  char *cp;
  int count, unit;
  char op;

  op = '\0';
  if ( ( *string == _PARSE_OP_ADD )
       || ( *string == _PARSE_OP_SUB ) ) {
    op = *string;
    string++;
  }

  count = strtol (string, &cp, 10);
  if ( cp == string ) {      /* No numeric argument */
    output_error ("Invalid format: %s", string);
    return -1;
  }

  /* remove whitespace */
  while ( strchr(WHITESPACE, *cp) ) cp++;


  if ( ! strncasecmp(cp, "s", 1) ) {
    unit = SEC;
  }
  else if ( ! strncasecmp(cp, "mi", 2) ) {
    unit = MIN;
  }
  else if ( ! strncasecmp(cp, "h", 1) ) {
    unit = HOUR;
  }
  else if ( ! strncasecmp(cp, "d", 1) ) {
    unit = DAY;
  }
  else if ( ! strncasecmp(cp, "w", 1) ) {
    unit = WEEK;
  }
  else if ( ! strncasecmp(cp, "mo", 2) ) {
    unit = MONTH;
  }
  else if (strchr(ABC, *cp)) {
     output_error ("Invalid format: %s", string);
     return -1;
  }
  else {
     unit = SEC;
  }

  switch (op) {
  case _PARSE_OP_ADD:
    return (time_t) orig + (count*unit);
  case _PARSE_OP_SUB:
    return (time_t) orig - (count*unit);
  default:
    return (time_t) count*unit;
  }

}

#define KILO  1024ul
#define MEGA  KILO * KILO
#define GIGA  KILO * MEGA
#define TERA  KILO * GIGA

#define KILO_10  1000ul
#define MEGA_10  KILO_10 * KILO_10
#define GIGA_10  KILO_10 * MEGA_10
#define TERA_10  KILO_10 * GIGA_10
/*
 * parse_size
 * understands Kb, Mb, Gb, Tb, bytes, and disk blocks
 * returns the number of bytes represented
 */
u_int64_t parse_size (u_int64_t orig, char *string, int parse_type) {
  char *cp;
  u_int64_t size, multiplier;
  double count;
  char op;

  op = '\0';
  if ( ( *string == _PARSE_OP_ADD )
       || ( *string == _PARSE_OP_SUB ) ) {
    op = *string;
    string++;
  }

  /* get the number */
  count = strtod(string, &cp);
  if (count == 0 && string == cp) {      // No numeric argument
    return orig;
  }

  /* negative sizes make no sense */
  if (count<0)
  {
    exit (ERR_ARG);
  }

  /* remove whitespace */
  while ( strchr(WHITESPACE, *cp) )  cp++;

  /* get the units */
  if ( ! strncasecmp(cp, "by", 2) ) {
    multiplier = 1;
  }
  else if ( ! strncasecmp(cp, "bl", 2) ) {
    multiplier = (parse_type == PARSE_BLOCKS ? BLOCK_SIZE : 1);
  }
  else if ( ! strncasecmp(cp, "k", 1) ) {
    multiplier = (parse_type == PARSE_BLOCKS ? KILO : KILO_10);
  }
  else if ( ! strncasecmp(cp, "m", 1) ) {
    multiplier = (parse_type == PARSE_BLOCKS ? MEGA : MEGA_10);
  }
  else if ( ! strncasecmp(cp, "g", 1) ) {
    multiplier = (parse_type == PARSE_BLOCKS ? GIGA : GIGA_10);
  }
  else if ( ! strncasecmp(cp, "t", 1) ) {
    multiplier = (parse_type == PARSE_BLOCKS ? TERA : TERA_10);
  }
  else {      // default to blocks or inodes
    multiplier = (parse_type == PARSE_BLOCKS ? BLOCK_SIZE : 1);
  }

  /* avoid a DIV0 */
  if (count == 0) {
    return 0;
  }

  /* calculate size (blocks or inodes) */
  if (parse_type == PARSE_BLOCKS) {
      size = (u_int64_t) (((double) count * multiplier - 1) / BLOCK_SIZE) + 1;
  }
  else { // inodes
      size = (u_int64_t) (count * multiplier);
  }

  switch (op) {
  case _PARSE_OP_ADD:
    return  orig + size;
  case _PARSE_OP_SUB:
    return  orig - size;
  default:
    return size;
  }

}
