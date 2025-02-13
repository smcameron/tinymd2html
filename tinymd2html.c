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

static void init_file_contents(struct file_contents *c)
{
	c->line = NULL;
	c->lines_allocated = 0;
	c->lines_used = 0;
}

static void usage(void)
{
	fprintf(stderr, "usage: tinymd2html -i input-file -o output-file\n");
	exit(1);
}

static void free_file_contents(struct file_contents *c)
{
	if (c->line) {
		for (int i = 0; i < c->lines_used; i++) {
			if (c->line[i])
				free(c->line[i]);
			c->line[i] = NULL;
		}
		free(c->line);
	}
	c->line = NULL;
	c->lines_allocated = 0;
	c->lines_used = 0;
}

static void trim_trailing_whitespace(char *line)
{
	int len = strlen(line);

	for (int i = len; i >= 0; i--) {
		if (line[i] != ' ' && line[i] != '\t' && line[i] != '\n')
			break;
		if (line[i] != '\n')
			line[i] = '\0';
	}
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

static void add_line_to_file_contents(struct file_contents *c, char *line)
{
	if (!c->line) {
		c->line = calloc(1024, sizeof(*c->line));
		for (int i = 0; i < 1024; i++)
			c->line[i] = NULL;
		c->lines_allocated = 1024;
		c->lines_used = 0;
	}
	if (c->lines_used == c->lines_allocated) {
		char **tmp = realloc(c->line, 2 * c->lines_allocated * sizeof(*c->line));
		if (!tmp) {
			fprintf(stderr, "Out of memory\n");
			exit(1);
		}
		c->line = tmp;
		c->lines_allocated *= 2;
	}
	c->line[c->lines_used] = strdup(line);
	c->lines_used++;
}

static int read_input_file(char *input_filename, struct file_contents *input)
{
	FILE *inputfile;

	free_file_contents(input);
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
		trim_trailing_whitespace(buffer);
		add_line_to_file_contents(input, buffer);
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

char *is_md_heading(char *line, int *heading)
{
	int i;
	int len = strlen(line);

	/* Skip leading whitespace */
	for (i = 0; i < len; i++)
		if (line[i] != ' ' && line[i] != '\t')
			break;

	int count = 0;
	for (; i < len; i++) {
		if (line[i] == '#')
			count++;
		else
			break;
	}

	/* Skip more whitespace */
	for (; i < len; i++)
		if (line[i] != ' ' && line[i] != '\t')
			break;

	if (count > 6)
		*heading = 6;
	*heading = count;
	return &line[i];
}

static void convert_md_headings_to_html(struct file_contents *input, struct file_contents *output)
{
	free_file_contents(output);
	for (int i = 0; i < input->lines_used; i++) {
		int heading = 0;
		char *input_heading = is_md_heading(input->line[i], &heading);
		if (!heading) {
			add_line_to_file_contents(output, input->line[i]);
		} else {
			char line[2048];
			int len = strlen(input_heading);

			/* Cut off trailing newline, if any. */
			if (input_heading[len - 1] == '\n')
				input_heading[len - 1] = '\0';

			snprintf(line, sizeof(line), "<h%d>%s</h%d>\n", heading, input_heading, heading);
			add_line_to_file_contents(output, line);
		}
	}
}

static void handle_triple_backquote(struct file_contents *input, struct file_contents *output)
{
	int last_line_was_newline = 0;
	int inside_preformat = 0;

	for (int i = 0; i < input->lines_used; i++) {
		if (strcmp(input->line[i], "\n") == 0) {
			last_line_was_newline = 1;
			add_line_to_file_contents(output, "\n");
		} else {
			if (!inside_preformat && last_line_was_newline && strcmp(input->line[i], "```\n") == 0) {
				inside_preformat = 1;
				add_line_to_file_contents(output, "<pre>\n");
			} else if (inside_preformat && strcmp(input->line[i], "```\n") == 0) {
				inside_preformat = 0;
				add_line_to_file_contents(output, "</pre>\n");
			} else {
				add_line_to_file_contents(output, input->line[i]);
			}
		}
	}
}

int main(int argc, char *argv[])
{
	char *input_filename, *output_filename;
	FILE *outputfile;
	struct file_contents input, output;

	init_file_contents(&input);
	init_file_contents(&output);

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
	convert_md_headings_to_html(&output, &input);
	free_file_contents(&output);
	handle_triple_backquote(&input, &output);
	free_file_contents(&input);

	dump_file_contents(outputfile, &output);
	free_file_contents(&output);


	fclose(outputfile);

	return 0;
}

