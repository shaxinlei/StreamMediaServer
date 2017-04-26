#include <iostream>
using namespace std;

int main(void)
{
	double cups = 4.5;
	int donuts = 6;
	
	cout << sizeof cups << endl;
	cout << sizeof donuts << endl;
	cout << "donuts address:" << &donuts << endl;
	cout << "cups address:" << &cups << endl;
	cin.get();

	return 0;
}