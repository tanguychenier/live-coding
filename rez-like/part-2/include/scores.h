#ifndef SCORES_H
#define SCORES_H

// the table of the best runs, kept in a small file in the player's home,
// one line per run, the points, the zone reached, the best chain and the
// name. it is read when the game starts and written each time a run lands
// on it
#define SCORES_MAX     10
#define NAME_MAX       8
#define SCORES_FILE    ".axon-scores"
#define SCORES_LINE    64

struct score {
	char name[NAME_MAX + 1];
	long points;
	int zone;             // the zone reached, ZONES for a run that went all the way
	int chain;
};

void scores_load(void);
void scores_save(void);
// the place a run would take, from zero, or -1 when it does not rank
int scores_rank(long points);
void scores_add(const char *name, long points, int zone, int chain);
int scores_count(void);
const struct score *scores_at(int place);

#endif
