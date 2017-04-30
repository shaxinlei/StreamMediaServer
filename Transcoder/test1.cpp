#include  <stdio.h>
#include <algorithm>

void main()
{
	int n = 0;
	scanf("%d", &n);

	unsigned char * p = (unsigned char*)&n;
	for (int i = 0; i < 4; i++)
	{
		printf("|%x", *(p + i));
	}
	scanf("%d", &n);

}
