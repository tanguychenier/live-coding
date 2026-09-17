#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scores.h"

static struct score table[SCORES_MAX];
static int count;

// the file lives in the home directory, or beside the game when there is
// no home to speak of
static const char *scores_path(void)
{
	static char path[SCORES_LINE * 4];
	const char *home = getenv("HOME");
	snprintf(path, sizeof path, "%s/%s", home ? home : ".", SCORES_FILE);
	return path;
}

void scores_load(void)
{
	count = 0;
	FILE *file = fopen(scores_path(), "r");
	if (!file)
		return;
	char line[SCORES_LINE];
	while (count < SCORES_MAX && fgets(line, sizeof line, file)) {
		struct score *score = &table[count];
		if (sscanf(line, "%ld %d %d %8s", &score->points, &score->zone, &score->chain,
			   score->name) == 4)
			count++;
	}
	fclose(file);
}

void scores_save(void)
{
	FILE *file = fopen(scores_path(), "w");
	if (!file)
		return;
	for (int i = 0; i < count; i++)
		fprintf(file, "%ld %d %d %s\n", table[i].points, table[i].zone, table[i].chain,
			table[i].name);
	fclose(file);
}

int scores_rank(long points)
{
	if (points <= 0)
		return -1;
	int place = 0;
	while (place < count && table[place].points >= points)
		place++;
	return place < SCORES_MAX ? place : -1;
}

void scores_add(const char *name, long points, int zone, int chain)
{
	int place = scores_rank(points);
	if (place < 0)
		return;
	if (count < SCORES_MAX)
		count++;
	for (int i = count - 1; i > place; i--)
		table[i] = table[i - 1];
	struct score *score = &table[place];
	memset(score, 0, sizeof *score);
	strncpy(score->name, name, NAME_MAX);
	if (!score->name[0])
		strcpy(score->name, "AXON");
	score->points = points;
	score->zone = zone;
	score->chain = chain;
}

int scores_count(void)
{
	return count;
}

const struct score *scores_at(int place)
{
	return &table[place < 0 ? 0 : place >= count ? count - 1 : place];
}
