/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#define CLIENT_SPACE

#include "clockwork.h"

#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "template.h"
#include "policy.h"

static struct cwt_client* cwt_options(int argc, char **argv);
static void show_version(void);
static void show_help(void);

struct cwt_client {
	int check_only;   /* don't render, just verify */
	char *template;   /* file to render */
	char *facts_from; /* where to read facts; NULL = standard */
	struct hash *facts;

	int log_level;
	char *config_file;
};

/**************************************************************/

int main(int argc, char **argv)
{
	struct cwt_client *cwt;
	struct template *tpl;

	cwt = cwt_options(argc, argv);
	cwt->log_level = cw_log_level(cwt->log_level, NULL);
	cw_log(LOG_INFO, "Log level is %s", cw_log_level_name(-1));

	cw_log(LOG_INFO, "Gathering facts");
	struct hash *facts = hash_new();
	const char *gatherers = "/lib/clockwork/gather.d/*"; /* FIXME! */
	if (cwt->facts_from) {
		cw_log(LOG_INFO, "Reading cached facts from %s", cwt->facts_from);
		if (fact_cat_read(cwt->facts_from, facts) != 0) {
			cw_log(LOG_CRIT, "Unable to read cached facts");
			exit(1);
		}
	} else if (fact_gather(gatherers, facts) != 0) {
		cw_log(LOG_INFO, "Gathering facts from %s", gatherers);
		cw_log(LOG_CRIT, "Unable to gather facts");
		exit(1);
	}
	if (cwt->facts) {
		cw_log(LOG_INFO, "Merging collected facts with command-line overrides");
		hash_merge(facts, cwt->facts);
	}

	tpl = template_create(cwt->template, facts);
	if (!tpl) {
		cw_log(LOG_CRIT, "Failed to parse template");
		exit(1);
	}
	if (cwt->check_only) {
		template_render(tpl);
		fprintf(stderr, "%s OK\n", cwt->template);
	} else {
		printf("%s\n", template_render(tpl));
	}

	return 0;
}

/**************************************************************/

static struct cwt_client* cwt_options(int argc, char **argv)
{
	struct cwt_client *cwt;

	const char *short_opts = "h?c:s:p:nvqF:f:DVC";
	struct option long_opts[] = {
		{ "help",     no_argument,       NULL, 'h' },
		{ "config",   required_argument, NULL, 'c' },
		{ "verbose",  no_argument,       NULL, 'v' },
		{ "quiet",    no_argument,       NULL, 'q' },
		{ "facts",    required_argument, NULL, 'F' },
		{ "fact",     required_argument, NULL, 'f' },
		{ "version",  no_argument,       NULL, 'V' },
		{ "check",    no_argument,       NULL, 'C' },
		{ 0, 0, 0, 0 },
	};

	int opt, idx = 0;

	cwt = xmalloc(sizeof(struct cwt_client));

	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch(opt) {
		case 'h':
		case '?':
			show_help();
			exit(0);
		case 'V':
			show_version();
			exit(0);
		case 'c':
			free(cwt->config_file);
			cwt->config_file = strdup(optarg);
			break;
		case 'v':
			cwt->log_level++;
			break;
		case 'q':
			cwt->log_level--;
			break;
		case 'F':
			free(cwt->facts_from);
			cwt->facts_from = strdup(optarg);
			break;
		case 'f':
			if (!cwt->facts) {
				cwt->facts = hash_new();
			}
			if (fact_parse(optarg, cwt->facts) != 0) {
				perror("fact_parse");
				exit(1);
			}
			break;
		case 'C':
			cwt->check_only = 1;
			break;
		}
	}

	if (optind > 0 && optind < argc) {
		cwt->template = strdup(argv[optind]);
	}

	return cwt;
}

static void show_version(void)
{
	printf("cwtemplate (Clockwork) " VERSION "\n"
	       "Copyright 2011-2014 James Hunt\n");
}

static void show_help(void)
{
	printf("USAGE: cwtemplate [OPTIONS]\n"
	       "\n"
	       "  -h, --help            Show this helpful message.\n"
	       "                        (for more in-depth help, check the man pages.)\n"
	       "\n"
	       "  -V, --version         Print version and copyright information.\n"
	       "\n"
	       "  -c, --config          Specify the path to an alternate configuration file.\n"
	       "\n"
	       "  -C, --check           Don't render the template, just check the syntax.\n"
	       "\n"
	       "  -F, --facts           Specify the path to a cached facts file.\n"
	       "\n"
	       "  -f, --fact K=V        Set a fact.  Can be given more than once.\n"
	       "\n");
}
