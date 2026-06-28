#include <cmoc.h>
#include <coco.h>

void runm(const char *filename)
{
    *((unsigned int *)0x2dd) = 0x4D22;
    strcpy((char *)0x2df, filename);
    *((unsigned int *)0xa6) = 0x2dd;

    asm
    {
        ldd     #$4D1C
        jmp     $AE75
    }
}

int main(void)
{
    initCoCoSupport();
    runm(isCoCo3 ? "ISS3" : "ISS");
    return 0;
}
