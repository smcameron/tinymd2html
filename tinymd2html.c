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


#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fwf_needed = 0;

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

struct list_stack_element {
	int ordered;
	int indenting_level;
	int indent_spacing;
	int current_number;
};

#define MAX_LIST_STACK_DEPTH 100
static struct list_stack {
	struct list_stack_element  e[MAX_LIST_STACK_DEPTH];
	int top;
} list_stack = { 0 };

static void init_list_stack(struct list_stack *s)
{
	memset(s->e, 0, sizeof(s->e));
	s->top = -1;
}

static void insert_styles(FILE *output)
{

	if (fwf_needed) {
		fprintf(output, "<style>\n");
		if (fwf_needed) {
			fprintf(output, "fwf {\n"); /* fixed width font */
			fprintf(output, "	font-family: courier\n");
			fprintf(output, "}\n");
		}
		fprintf(output, "</style>\n");
	}
}

static void push_list_stack(struct list_stack *s, int ordered, int indent_spacing)
{
	int indent;

	if (s->top >= MAX_LIST_STACK_DEPTH) {
		fprintf(stderr, "List stack too big.\n");
		exit(1);
	}

	if (s->top < 0)
		indent = 0;
	else
		indent = s->e[s->top].indenting_level + 1;

	s->top++;
	s->e[s->top].indenting_level = indent;
	s->e[s->top].current_number = 1;
	s->e[s->top].ordered = ordered;
	s->e[s->top].indent_spacing = indent_spacing;
	// printf("push indent = %d, ordered = %d\n", indent_spacing, ordered);
}

static void pop_list_stack(struct list_stack *s)
{
	if (s->top < 0) {
		fprintf(stderr, "List stack underflow\n");
		exit(1);
	}
	// printf("pop indent = %d, ordered = %d\n", s->e[s->top].indent_spacing, s->e[s->top].ordered);
	s->top--;
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

static void trim_trailing_newline(char *line)
{
	int len = strlen(line);
	if (line[len - 1] == '\n')
		line[len - 1] = '\0';
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
	else
		*heading = count;
	return &line[i];
}

static void format_nametag(char *nametag)
{
	int i, j;

	i = 0; j = 0;
	while (1) {
		if (nametag[j] == '\0') {
			nametag[i] = '\0';
			break;
		}
		if (nametag[j] >= 'A' && nametag[j] <= 'Z') {
			nametag[i] = tolower(nametag[j]);
			i++; j++;
			continue;
		}
		if (nametag[j] == ' ' || nametag[j] == '\t') {
			j++;
			continue;
		}
		if ((nametag[j] >= 'a' && nametag[j] <= 'z') ||
			(nametag[j] >= '0' && nametag[j] <= '9')) {
			nametag[i] = nametag[j];
			i++; j++;
			continue;
		}
		j++;
	}
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
			char line[4096];
			int len = strlen(input_heading);
			char *nametag = strdup(input_heading);

			format_nametag(nametag);

			/* Cut off trailing newline, if any. */
			if (input_heading[len - 1] == '\n')
				input_heading[len - 1] = '\0';

			snprintf(line, sizeof(line), "<a name=\"%s\"><h%d>%s</h%d></a>\n",
				nametag, heading, input_heading, heading);
			free(nametag);
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

/* Returns 1 if line looks like an list element, and fills
 * in element position with the column of the element */
static int looks_like_list_element(char *line, int *element_position, int *ordered)
{
	int number;

	/* See if it looks like an ordered list element */
	int rc = sscanf(line, " %d[.] ", &number);
	if (rc == 1) {
		char *x = strchr(line, '.');
		if (!x)
			return 0;
		*element_position = x - line;
		int len = strlen(line);
		if (*element_position <= len - 3) {
			*element_position += 2;
			*ordered = 1;
		}
		// printf("%s looks like a list, element position = %d\n", line, *element_position);
		return 1;
	}

	/* See if it looks like an unordered list element */
	char c[1024];
	rc = sscanf(line, " %1023[*-] %*s", c);
	if (rc == 1) {
		char *x = strchr(line, c[0]);
		if (!x)
			return 0;
		*element_position = x - line + 2;
		if ((size_t) *element_position < strlen(line)) {
			*ordered = 0;
			// printf("%s looks like a list, element_position = %d\n", line, *element_position);
			return 1;
		}
	}
	// printf("%s not a list\n", line);
	return 0;
}

static void process_lists(struct file_contents *input, struct file_contents *output)
{
	int last_line_was_newline = 0;
	int looks_like_list = 0;
	int ordered;

	for (int i = 0; i < input->lines_used; i++) {
		int element_position;
		if (strcmp (input->line[i], "\n") == 0) {
			last_line_was_newline = 1;
			while (list_stack.top >= 0) {
				add_line_to_file_contents(output,
					list_stack.e[list_stack.top].ordered ? "</ol>\n" : "</ul>\n");
				pop_list_stack(&list_stack);
				if (list_stack.top >= 0) {
					element_position = list_stack.e[list_stack.top].indent_spacing;
					ordered = list_stack.e[list_stack.top].ordered;
				}
			}
			add_line_to_file_contents(output, "\n");
		} else {
			looks_like_list = looks_like_list_element(input->line[i], &element_position, &ordered);
			if (last_line_was_newline && looks_like_list) {
				char line[2048];
				/* Starting a new list element */
				add_line_to_file_contents(output, ordered ? "<ol>\n" : "<ul>\n");
				snprintf(line, sizeof(line), "<li>%s", &input->line[i][element_position]);
				add_line_to_file_contents(output, line);
				push_list_stack(&list_stack, ordered, element_position);
				last_line_was_newline = 0;
			} else if (looks_like_list) {
				char line[2048];
				int elpo = element_position;
				int indent_spacing;

				if (list_stack.top >= 0)
					indent_spacing = list_stack.e[list_stack.top].indent_spacing;
				else
					indent_spacing = 0;

				while (elpo > indent_spacing) {
					push_list_stack(&list_stack, ordered, element_position);
					add_line_to_file_contents(output, ordered ? "<ol>\n" : "<ul>\n");
					elpo = list_stack.e[list_stack.top].indent_spacing;
					ordered = list_stack.e[list_stack.top].ordered;
					if (list_stack.top >= 0)
						indent_spacing = list_stack.e[list_stack.top].indent_spacing;
					else
						indent_spacing = 0;
				}

				while (elpo < list_stack.e[list_stack.top].indent_spacing) {
					ordered = list_stack.e[list_stack.top].ordered;
					add_line_to_file_contents(output, ordered ? "</ol>\n" : "</ul>\n");
					pop_list_stack(&list_stack);
				}
				if (list_stack.top >= 0) {
					elpo = list_stack.e[list_stack.top].indent_spacing;
					ordered = list_stack.e[list_stack.top].ordered;
				}
				snprintf(line, sizeof(line), "<li>%s", &input->line[i][element_position]);
				// snprintf(line, sizeof(line), "<li>%s", input->line[i]);
				add_line_to_file_contents(output, line);
				last_line_was_newline = 0;
			} else { /* does not look like a list element */
				int indent_spacing = 0;
				int j;

				if (list_stack.top >= 0)
					indent_spacing = list_stack.e[list_stack.top].indent_spacing;
				for (j = 0; input->line[i][j] == ' '; )
					j++;

				if (input->line[i][j] != '\0')
				while (indent_spacing >	j) {
					ordered = list_stack.e[list_stack.top].ordered;
					add_line_to_file_contents(output, ordered ? "</ol>\n" : "</ul>\n");
					pop_list_stack(&list_stack);
					if (list_stack.top >= 0) {
						indent_spacing = list_stack.e[list_stack.top].indent_spacing;
						ordered = list_stack.e[list_stack.top].ordered;
					} else {
						indent_spacing = 0;
						ordered = 0;
					}
					if (list_stack.top == 0)
						break;
				}
				add_line_to_file_contents(output, input->line[i]);
				last_line_was_newline = 0;
			}
		}
	}
}

static void process_paragraphs(struct file_contents *input, struct file_contents *output)
{
	free_file_contents(output);

	int paragraph_starting = 0;
	for (int i = 0; i < input->lines_used; i++) {
		if (strcmp(input->line[i], "\n") == 0) {
			paragraph_starting = 1;
			add_line_to_file_contents(output, "\n");
			continue;
		}
		if (paragraph_starting) {
			char line[2048];
			snprintf(line, sizeof(line), "<p>%s", input->line[i]);
			add_line_to_file_contents(output, line);
			paragraph_starting = 0;
		} else {
			add_line_to_file_contents(output, input->line[i]);
			paragraph_starting = 0;
		}
	}
}

static void process_boldface(struct file_contents *input, struct file_contents *output)
{
	int bold = 0;
	int p = 0;

	for (int i = 0; i < input->lines_used; i++) {
		char tmp[2][2048];

		snprintf(tmp[p], 2048, "%s", input->line[i]);
		do {
			char *x = strstr(tmp[p], "**");
			if (!x) {
				add_line_to_file_contents(output, tmp[p]);
				break;
			}
			p = !p;
			*x = '\0';
			snprintf(tmp[p], sizeof(tmp[p]), "%s%s%s", tmp[!p], bold ? "</b>" : "<b>", x + 2);
			bold = !bold;
		} while (1);
	}
}

static void process_single_backquotes(struct file_contents *input, struct file_contents *output)
{
	int inside_quotes = 0;
	int p = 0;

	for (int i = 0; i < input->lines_used; i++) {
		char tmp[2][2048];

		snprintf(tmp[p], sizeof(tmp[p]), "%s", input->line[i]);
		do {
			char *x = strstr(tmp[p], "`");
			if (!x) {
				add_line_to_file_contents(output, tmp[p]);
				break;
			}
			fwf_needed = 1;
			p = !p;
			*x = '\0';
			snprintf(tmp[p], sizeof(tmp[p]), "%s%s%s", tmp[!p], inside_quotes ? "</fwf>" : "<fwf>", x + 1);
			inside_quotes = !inside_quotes;
		} while (1);
	}
}

static void process_links(struct file_contents *input, struct file_contents *output)
{
	char url[2048];
	char text[2048];
	int is_image = 0;
	char current_input_line[8192];

	for (int i = 0; i < input->lines_used; i++) {
		snprintf(current_input_line, sizeof(current_input_line), "%s", input->line[i]);
		char *x;
check_if_more_links:
		x = strstr(current_input_line, "](");
		if (!x) { /* No link present, just copy output */
			add_line_to_file_contents(output, current_input_line);
			continue;
		}
		char *begin;
		for (begin = x; begin > current_input_line; begin--) {
			if (*begin == '[')
				break;
		}
		if (begin > current_input_line && *(begin - 1) == '!') {
			is_image = 1;
			*(begin - 1) = ' ';
		}
		if (begin == current_input_line && *begin != '[') { /* not a link, just copy */
			add_line_to_file_contents(output, current_input_line);
			continue;
		}
		char *end;
		for (end = x + 1; *end != '\0'; end++) {
			if (*end == ')') {
				break;
			}
		}
		if (*end == '\0') { /* not a link */
			add_line_to_file_contents(output, current_input_line);
			continue;
		}
		{
			int j = 0;
			char *c;
			for (c = begin + 1; *c != ']'; c++) {
				text[j++] = *c;
				if (j >= (int) sizeof(text)) {
					fprintf(stderr, "line %d: text associated with URL truncated\n", i);
					j = sizeof(text) - 1;
					break;
				}
			}
			text[j] = '\0';

			j = 0;
			for (char *c = x + 2; *c != ')'; c++) {
				url[j++] = *c;
				if (j >= (int) sizeof(url)) {
					fprintf(stderr, "line %d: URL truncated\n", i);
					j = sizeof(url) - 1;
					break;
				}
			}
			url[j] = '\0';

			char line[8192];

			trim_trailing_newline(current_input_line);
			*begin = '\0';
			if (is_image)
				snprintf(line, sizeof(line), "%s<img src=\"%s\" alt=\"%s\">%s\n",
					current_input_line, url, text, end + 1);
			else
				snprintf(line, sizeof(line), "%s<a href=\"%s\">%s</a>%s\n",
					current_input_line, url, text, end + 1);
			snprintf(current_input_line, sizeof(current_input_line), "%s", line);
			goto check_if_more_links;
		}
	}
}

static int count_blockquote(char *line, char **start)
{
	int i;
	int len = strlen(line);

	int rc = 0;
	i = 0;
	if (strncmp("<p>", line, 3) == 0)
		i = 3;
	for (; i < len; i += 4) {
		if (strncmp("&gt;", &line[i], 4) == 0)
			rc++;
		else
			break;
	}
	*start = &line[i];
	return rc;
}

static void process_blockquote(struct file_contents *input, struct file_contents *output)
{
	static int blockquote_level = 0;
	char *start = NULL;

	// printf("blockquote = %d\n", blockquote_level);
	for (int i = 0; i < input->lines_used; i++) {
		// printf("input = '%s'\n", input->line[i]);
		if (strcmp(input->line[i], "\n") == 0) {
			for (; blockquote_level > 0; blockquote_level--)
				add_line_to_file_contents(output, "</blockquote>\n");
			add_line_to_file_contents(output, "\n");
			continue;
		}
		int bql = count_blockquote(input->line[i], &start);
		// printf("bql = %d, blockquote = %d\n", bql, blockquote_level);
		if (bql != blockquote_level) {
			if (bql > blockquote_level) {
				// printf("up bql = %d, bq = %d %s\n", bql, blockquote_level, input->line[i]);
				for (; blockquote_level < bql; blockquote_level++) {
					add_line_to_file_contents(output, "<blockquote>\n");
				}
				// printf("blockquote = %d\n", blockquote_level);
			} else {
				// printf("down bql = %d, bq = %d %s\n", bql, blockquote_level, input->line[i]);
				for (; bql < blockquote_level; blockquote_level--) {
					add_line_to_file_contents(output, "</blockquote>\n");
				}
				// printf("blockquote = %d\n", blockquote_level);
			}
		}
		if (bql == 0)
			start = input->line[i];
		add_line_to_file_contents(output, start);
	}
}

int main(int argc, char *argv[])
{
	char *input_filename, *output_filename;
	FILE *outputfile;
	struct file_contents pingpong[2];
	int p = 0;

	init_list_stack(&list_stack);
	init_file_contents(&pingpong[0]);
	init_file_contents(&pingpong[1]);

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

	read_input_file(input_filename, &pingpong[p]);

	escape_html_specials(&pingpong[p], &pingpong[!p]);
	free_file_contents(&pingpong[p]); p = !p;

	convert_md_headings_to_html(&pingpong[p], &pingpong[!p]);
	free_file_contents(&pingpong[p]); p = !p;

	handle_triple_backquote(&pingpong[p], &pingpong[!p]);
	free_file_contents(&pingpong[p]); p = !p;

	process_boldface(&pingpong[p], &pingpong[!p]);
	free_file_contents(&pingpong[p]); p = !p;

	process_single_backquotes(&pingpong[p], &pingpong[!p]);
	free_file_contents(&pingpong[p]); p = !p;

	process_lists(&pingpong[p], &pingpong[!p]);
	free_file_contents(&pingpong[p]); p = !p;

	process_paragraphs(&pingpong[p], &pingpong[!p]);
	free_file_contents(&pingpong[p]); p = !p;

	process_links(&pingpong[p], &pingpong[!p]);
	free_file_contents(&pingpong[p]); p = !p;

	process_blockquote(&pingpong[p], &pingpong[!p]);
	free_file_contents(&pingpong[p]); p = !p;

	insert_styles(outputfile);
	dump_file_contents(outputfile, &pingpong[p]);
	free_file_contents(&pingpong[p]);

	fclose(outputfile);

	return 0;
}

