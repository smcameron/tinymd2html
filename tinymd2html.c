/*
	Copyright (C) 2025 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of tinymd2html.

	tinymd2html is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	tinymd2html is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with tinymd2html; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct file_contents {
	char **line;
	int lines_allocated, lines_used;
};

static void usage(void)
{
	fprintf(stderr, "usage: tinymd2html -i input-file -o output-file\n");
	exit(1);
}

static void parse_options(int argc, char *argv[], char **input_filename, char **output_filename)
{
	int c;

	*input_filename = NULL;
	*output_filename = NULL;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"input", required_argument, 0,  'i' },
			{"output",required_argument, 0,  'o' },
			{0, 0, 0, 0 },
		};

		c = getopt_long(argc, argv, "i:o:", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'i':
			*input_filename = optarg;
			break;
		case 'o':
			*output_filename = optarg;
			break;
		default:
			usage();
			break;
		}
	}
}

static int read_input_file(char *input_filename, struct file_contents *input)
{
	FILE *inputfile;

	input->line = calloc(1024, sizeof(*input->line));
	for (int i = 0; i < 1024; i++)
		input->line[i] = NULL;
	input->lines_allocated = 1024;
	input->lines_used = 0;

	if (input_filename == NULL || strcmp(input_filename, "-") == 0) {
		inputfile = stdin;
	} else {
		inputfile = fopen(input_filename, "r");
		if (!inputfile) {
			fprintf(stderr, "Failed to open %s for input: %s\n", input_filename, strerror(errno));
			exit(1);
		}
	}

	do {
		char buffer[1024];

		char *l = fgets(buffer, sizeof(buffer), inputfile); /* Read a line from input file into buffer */
		if (!l) {
			if (feof(inputfile))
				break;
			fprintf(stderr, "Error reading file %s at line %d\n", input_filename, input->lines_used + 1);
				break;
		}

		/* Allocate more lines if need be */
		if (input->lines_used == input->lines_allocated) {
			char **tmp = realloc(input->line, 2 * input->lines_allocated * sizeof(*input->line));
			if (!tmp) {
				fprintf(stderr, "Out of memory reading input file at line %d\n", input->lines_used + 1);
				exit(1);
			}
			input->line = tmp;
			input->lines_allocated *= 2;
		}
		input->line[input->lines_used] = strdup(buffer);
		input->lines_used++;
	} while (1);
	fclose(inputfile);
	return 0;
}

static void dump_file_contents(FILE *f, struct file_contents *input)
{
	for (int i = 0; i < input->lines_used; i++)
		fprintf(f, "%s", input->line[i]);
}

static void escape_html_specials_line(char *input_line, char **output_line)
{
	/* Calculate how long the output line needs to be. */
	int input_len = strlen(input_line);
	int output_len = input_len + 1;

	for (int i = 0; i < input_len; i++) {
		if (input_line[i] == '<')
			output_len += 3; /* '<' --> '&lt;' */
		else if (input_line[i] == '>')
			output_len += 3; /* '>' --> '&gt;' */
		else if (input_line[i] == '&')
			output_len += 4; /* '&' --> '&amp;' */
	}
	*output_line = calloc(output_len, 1);
	int o = 0;
	for (int i = 0; i < input_len; i++) {
		if (input_line[i] == '<') {
			(*output_line)[o] = '&'; o++;
			(*output_line)[o] = 'l'; o++;
			(*output_line)[o] = 't'; o++;
			(*output_line)[o] = ';'; o++;
		} else if (input_line[i] == '>') {
			(*output_line)[o] = '&'; o++;
			(*output_line)[o] = 'g'; o++;
			(*output_line)[o] = 't'; o++;
			(*output_line)[o] = ';'; o++;
		} else if (input_line[i] == '&') {
			(*output_line)[o] = '&'; o++;
			(*output_line)[o] = 'a'; o++;
			(*output_line)[o] = 'm'; o++;
			(*output_line)[o] = 'p'; o++;
			(*output_line)[o] = ';'; o++;
		} else {
			(*output_line)[o] = input_line[i];
			o++;
		}
	}
	(*output_line)[o] = '\0';
}

static void escape_html_specials(struct file_contents *input, struct file_contents *output)
{

	output->line = calloc(input->lines_used, sizeof(*output->line));
	if (!output->line) {
		fprintf(stderr, "Out of memory processing html special chars.\n");
		exit(1);
	}
	output->lines_allocated = input->lines_used;
	output->lines_used = 0;
	for (int i = 0; i < input->lines_used; i++) {
		char *output_line;

		escape_html_specials_line(input->line[i], &output_line);
		output->line[i] = output_line;
		output_line = NULL;
		output->lines_used++;
	}
}

int main(int argc, char *argv[])
{
	char *input_filename, *output_filename;
	FILE *outputfile;
	struct file_contents input, output;

	parse_options(argc, argv, &input_filename, &output_filename);
	if (output_filename == NULL || strcmp(output_filename, "-") == 0) {
		outputfile = stdout;
	} else {
		outputfile = fopen(output_filename, "w+");
		if (!outputfile) {
			fprintf(stderr, "Failed to open %s for output: %s\n", output_filename, strerror(errno));
			exit(1);
		}
	}

	read_input_file(input_filename, &input);

	escape_html_specials(&input, &output);

	dump_file_contents(outputfile, &output);

	fclose(outputfile);

	return 0;
}

