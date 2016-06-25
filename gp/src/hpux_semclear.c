void _gp_semclear(int *word)
{
    _flush_globals();
    *((volatile int *) word) = 1;
#if !defined(__ia64)
    _asm("SYNC");
#endif
}

