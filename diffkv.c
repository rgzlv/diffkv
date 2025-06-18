#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Key-value pair that should be ignored in subsequent comparisons
#define KV_IGNORE ((struct kv *)1)

struct kv {
	char	*k;
	char	*v;
};

enum line_type {
	LINE_TYPE_EMPTY,	// Only newline or EOF
	LINE_TYPE_WS,		// Only whitespace
	LINE_TYPE_COMMENT,	// First non-whitespace character is '#'
	LINE_TYPE_TEXT,		// Non-empty, non-whitespace, non-comment
};

char *
getln(FILE *f)
{
	char *ln = NULL;
	unsigned long sz = 0;

	for (;;) {
		int c;

		c = fgetc(f);
		if (c == EOF || c == '\n')
			break;
		ln = realloc(ln, ++sz);
		if (!ln)
			_Exit(2);
		ln[sz - 1] = c;
	}
	if (ferror(f)) {
		perror("file read error");
		exit(1);
	}
	if (!sz)
		return NULL;
	ln = realloc(ln, sz + 1);
	ln[sz] = 0;
	return ln;
}

enum line_type
getlntype(char *ln)
{
	char *c;

	if (!ln)
		return LINE_TYPE_EMPTY;
	for (c = ln; *c; c++) {
		if (isspace(*c))
			continue;
		if (*c == '#')
			return LINE_TYPE_COMMENT;
		return LINE_TYPE_TEXT;
	}
	return LINE_TYPE_WS;
}

char *
getnonwsln(FILE *f)
{
	char *ln;

	for (;;) {
		enum line_type lntype;

		ln = getln(f);
		lntype = getlntype(ln);
		if (lntype == LINE_TYPE_TEXT)
			return ln;
		free(ln);
		if (feof(f))
			break;
	}
	return NULL;
}

struct kv **
kvs_from_file(FILE *f)
{
	struct kv **kvs = NULL;
	unsigned long sz = 0;

	for (;;) {
		char *ln;
		char *c;
		char *eq = NULL;
		struct kv *kv;

		ln = getnonwsln(f);
		if (!ln)
			break;
		kvs = realloc(kvs, sizeof(*kvs) * ++sz);
		if (!kvs)
			_Exit(2);
		kvs[sz - 1] = malloc(sizeof(*kvs[sz - 1]));
		if (!kvs[sz - 1])
			_Exit(2);
		kv = kvs[sz - 1];

		// Skip whitespace between BOL and key
		c = ln;
		while (isspace(*c))
			c++;
		kv->k = c;

		// Skip whitespace between key and '=', and '=' itself
		while (!isspace(*c) && *c != '=')
			c++;
		if (*c == '=')
			eq = c;
		if (!eq) {
			eq = strchr(c, '=');
			if (!eq) {
				fprintf(stderr, "diffkv: missing '=' in \"%s\"\n", ln);
				exit(1);
			}
		}
		*c = 0;

		// Skip whitespace between '=' and value
		c = eq + 1;
		while (isspace(*c))
			c++;
		kv->v = c;

		// Empty value, e.g. "key="
		if (*c == 0)
			continue;

		// Skip whitespace between value and EOL
		while (*c)
			c++;
		--c;
		while (isspace(*c))
			c--;
		*++c = 0;
	}
	if (!sz)
		return NULL;
	kvs = realloc(kvs, sizeof(*kvs) * (sz + 1));
	if (!kvs)
		_Exit(2);
	kvs[sz] = NULL;
	return kvs;
}

FILE *
file_from_arg(char *arg)
{
	FILE *f;

	if (!strcmp(arg, "-"))
		f = stdin;
	else {
		f = fopen(arg, "rb");
		if (!f) {
			perror(arg);
			exit(1);
		}
	}
	return f;
}

struct kv **
kvs_find_key(struct kv **kvs, char *key)
{
	for (; *kvs; kvs++) {
		if (*kvs == KV_IGNORE)
			continue;
		if (!strcmp((*kvs)->k, key))
			return kvs;
	}
	return NULL;
}

void
diff(struct kv **kvs1, struct kv **kvs2)
{
	struct kv **kv1;
	struct kv **kv2;
	struct kv **match;

	for (kv1 = kvs1; *kv1; kv1++) {
		match = kvs_find_key(kvs2, (*kv1)->k);
		if (!match)
			printf("< %s\n", (*kv1)->k);
		else if (strcmp((*kv1)->v, (*match)->v))
			printf("! %s=%s > %s\n", (*kv1)->k, (*kv1)->v, (*match)->v);
		free(*kv1);
		*kv1 = KV_IGNORE;
		free(*match);
		*match = KV_IGNORE;
	}

	for (kv2 = kvs2; *kv2; kv2++) {
		if (*kv2 == KV_IGNORE)
			continue;
		printf("> %s=%s\n", (*kv2)->k, (*kv2)->v);
	}
}

int
main(int argc, char **argv)
{
	struct kv **kvs1;
	struct kv **kvs2;

	argc--;
	argv++;
	if (!argc || !(argc - 1)) {
		fprintf(stderr, "diffkv: missing file argument(s)\n");
		goto usage;
	}
	if (argc - 2) {
		fprintf(stderr, "diffkv: extra file argument(s): \"%s\"\n", *(argv + 2));
		goto usage;
	}
	kvs1 = kvs_from_file(file_from_arg(*argv));
	argc--;
	argv++;
	kvs2 = kvs_from_file(file_from_arg(*argv));
	diff(kvs1, kvs2);
	return 0;
usage:
	fprintf(stderr, "usage: diffkv file1 file2\n");
	return 1;
}
