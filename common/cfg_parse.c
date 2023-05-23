#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char    *cfg = NULL;
//twice 0:end

char           *
getcfg(const char *tag)
{
	if (cfg == NULL) {
		FILE           *fp = fopen("/data/system.cfg", "r");
		if (fp == NULL) {
			return NULL;
		}
		char           *bmax = malloc(4096);
		int		l = fread(bmax, 1, 4096, fp);
		fclose(fp);
		cfg = malloc(l + 2);
		memcpy(cfg, bmax, l);
		cfg[l] = 0;
		cfg[l + 1] = 0;
		for (char *p = cfg; *p; p++) {
			if (*p == '\n')
				*p++ = 0;
		}
		free(bmax);
	}
	int		wlen = strlen(tag);
	for (char *p = cfg; *p; p += strlen(p) + 1) {
		if (strncmp(tag, p, wlen) == 0 && p[wlen] == '=')
			return p + wlen + 1;
	}
	return NULL;
}


#ifdef TEST
int 
main(int argc, char *argv[])
{
	while (argc-- > 1) {
		char           *s = getcfg(*++argv);
		printf("R: %s ->%s<\n", *argv, s);
	}
	return 0;
}
#endif
