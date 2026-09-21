
#include <stdio.h>
#include <unistd.h>
#include "bcm2835.h" // https://www.airspayce.com/mikem/bcm2835/
#include <chrono>
#include <thread>
#include <cmath>
#include <vector>
#include <sys/time.h>
#include <signal.h>

#define JJY_OUT_PIN RPI_V2_GPIO_P1_03
#define FREQ_PIN RPI_V2_GPIO_P1_07
#define RESET_PIN RPI_V2_GPIO_P1_08


class JJY {
private:
	// 初期化
	void init_i2c()
	{
		bcm2835_gpio_fsel(RESET_PIN, BCM2835_GPIO_FSEL_OUTP);
		bcm2835_gpio_set(RESET_PIN);
		//
		bcm2835_gpio_fsel(FREQ_PIN, BCM2835_GPIO_FSEL_OUTP);
//		bcm2835_gpio_set(FREQ_PIN); // 60kHz
		bcm2835_gpio_clr(FREQ_PIN); // 40kHz
		// ピンを入力モードにする
		bcm2835_gpio_fsel(JJY_OUT_PIN, BCM2835_GPIO_FSEL_INPT);
		// ピンをLOWにする
		bcm2835_gpio_clr(JJY_OUT_PIN);
	}

public:
	JJY()
	{
		init_i2c();
	}

	void out_lo()
	{
		// SCLピンを出力モードにするとピンの状態がLOWになる
		bcm2835_gpio_fsel(JJY_OUT_PIN, BCM2835_GPIO_FSEL_OUTP);
	}

	void out_hi()
	{
		// SCLピンを入力モードにするとピンの状態がHIGHになる
		bcm2835_gpio_fsel(JJY_OUT_PIN, BCM2835_GPIO_FSEL_INPT);
	}

};


JJY *wire;





namespace {

unsigned long convert_ymd_to_cjd(int year, int month, int day)
{
	if (month < 3) {
		month += 9;
		year--;
	} else {
		month -= 3;
	}
	year += 4800;
	int c = year / 100;
	return c * 146097 / 4 + (year - c * 100) * 1461 / 4 + (153 * month + 2) / 5 + day - 32045;
}

void convert_cjd_to_ymd(unsigned long j, int *year, int *month, int *day)
{
	int y, m, d;
	y = (j * 4 + 128179) / 146097;
	d = (j * 4 - y * 146097 + 128179) / 4 * 4 + 3;
	j = d / 1461;
	d = (d - j * 1461) / 4 * 5 + 2;
	m = d / 153;
	d = (d - m * 153) / 5 + 1;
	y = (y - 48) * 100 + j;
	if (m < 10) {
		m += 3;
	} else {
		m -= 9;
		y++;
	}
	*year = y;
	*month = m;
	*day = d;
}

bool parity(uint64_t bits)
{
	uint64_t l, h;
	l = bits & 0x5555555555555555;
	h = (bits & 0xaaaaaaaaaaaaaaaa) >> 1;
	bits = l + h;
	l = bits & 0x3333333333333333;
	h = (bits & 0xcccccccccccccccc) >> 2;
	bits = l + h;
	l = bits & 0x0f0f0f0f0f0f0f0f;
	h = (bits & 0xf0f0f0f0f0f0f0f0) >> 4;
	bits = l + h;
	l = bits & 0x00ff00ff00ff00ff;
	h = (bits & 0xff00ff00ff00ff00) >> 8;
	bits = l + h;
	l = bits & 0x0000ffff0000ffff;
	h = (bits & 0xffff0000ffff0000) >> 16;
	bits = l + h;
	l = bits & 0x00000000ffffffff;
	h = (bits & 0xffffffff00000000) >> 32;
	bits = l + h;
	return bits & 1;
}

enum class Playing : uint8_t {
	Marker,
	Value0,
	Value1,
};

struct DateTime {
	int year = 0;
	int month = 0;
	int day = 0;
	int hour = 0;
	int minute = 0;
	int second = 0;
	int ms = 0;
};

void make_data(DateTime const &dt, std::vector<Playing> *out)
{
	out->clear();
	out->reserve(60);

	unsigned long day = convert_ymd_to_cjd(dt.year, dt.month, dt.day);
	int wd = (day + 1) % 7;
	day = day - convert_ymd_to_cjd(dt.year, 1, 1) + 1;

	uint64_t bits;
	auto push = [&](bool v){
		bits <<= 1;
		if (v) bits |= 1;
	};

	/* 01/52 */ push((dt.minute / 10) & 4);
	/* 02/51 */ push((dt.minute / 10) & 2);
	/* 03/50 */ push((dt.minute / 10) & 1);
	/* 04/49 */ push(false);
	/* 05/48 */ push((dt.minute % 10) & 8);
	/* 06/47 */ push((dt.minute % 10) & 4);
	/* 07/46 */ push((dt.minute % 10) & 2);
	/* 08/45 */ push((dt.minute % 10) & 1);

	/* 10/44 */ push(false);
	/* 11/43 */ push(false);
	/* 12/42 */ push((dt.hour / 10) & 2);
	/* 13/41 */ push((dt.hour / 10) & 1);
	/* 14/40 */ push(false);
	/* 15/39 */ push((dt.hour % 10) & 8);
	/* 16/38 */ push((dt.hour % 10) & 4);
	/* 17/37 */ push((dt.hour % 10) & 2);
	/* 18/36 */ push((dt.hour % 10) & 1);

	/* 20/35 */ push(false);
	/* 21/34 */ push(false);
	/* 22/33 */ push((day / 100) & 2);
	/* 23/32 */ push((day / 100) & 1);
	/* 24/31 */ push(false);
	/* 25/30 */ push((day / 10 % 10) & 8);
	/* 26/29 */ push((day / 10 % 10) & 4);
	/* 27/28 */ push((day / 10 % 10) & 2);
	/* 28/27 */ push((day / 10 % 10) & 1);

	/* 30/26 */ push((day % 10) & 8);
	/* 31/25 */ push((day % 10) & 4);
	/* 32/24 */ push((day % 10) & 2);
	/* 33/23 */ push((day % 10) & 1);
	/* 34/22 */ push(false);
	/* 35/21 */ push(false);
	/* 36/20 */ push(false); // p1
	/* 37/19 */ push(false); // p2
	/* 38/18 */ push(false);

	/* 40/17 */ push(false);
	/* 41/16 */ push((dt.year / 10 % 10) & 8);
	/* 42/15 */ push((dt.year / 10 % 10) & 4);
	/* 43/14 */ push((dt.year / 10 % 10) & 2);
	/* 44/13 */ push((dt.year / 10 % 10) & 1);
	/* 45/12 */ push((dt.year % 10) & 8);
	/* 46/11 */ push((dt.year % 10) & 4);
	/* 47/10 */ push((dt.year % 10) & 2);
	/* 48/09 */ push((dt.year % 10) & 1);

	/* 50/08 */ push(wd & 4);
	/* 51/07 */ push(wd & 2);
	/* 52/06 */ push(wd & 1);
	/* 53/05 */ push(false);
	/* 54/04 */ push(false);
	/* 55/03 */ push(false);
	/* 56/02 */ push(false);
	/* 57/01 */ push(false);
	/* 58/00 */ push(false);

	if (parity(bits & (0xffLL << 45))) bits |= 1 << 19;
	if (parity(bits & (0x7fLL << 36))) bits |= 1 << 20;

	for (int i = 0; i < 60; i++) {
		if (i == 0 || i % 10 == 9) {
			out->push_back(Playing::Marker);
		} else {
			bool v = bits & (1LL << 52);
			out->push_back(v ? Playing::Value1 : Playing::Value0);
			bits <<= 1;
		}
	}
}

} // namespace



void sigint(int i)
{
	wire->out_lo();
	bcm2835_close();
	exit(0);
}

void getCurrentDateTime(DateTime *dt)
{
	time_t t = time(nullptr);
	auto *tm = localtime(&t);
	std::chrono::system_clock::duration d = std::chrono::system_clock::now().time_since_epoch();
	long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
	ms += tm->tm_gmtoff * 1000;
	dt->ms = ms % 1000;
	dt->second = ms / 1000 % 60;
	dt->minute = ms / 60000 % 60;
	dt->hour = ms / 3600000 % 24;
	long long j = ms / 86400000 + 2440588;
	convert_cjd_to_ymd(j, &dt->year, &dt->month, &dt->day);
}

void setup()
{
	bcm2835_init();
	wire = new JJY();
}

void msleep(int ms)
{
	usleep(ms * 1000);
}

void loop()
{
#if 0
	wire->out_hi();
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	wire->out_lo();
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
#else
	static int sec = -1;
	static int dur = 0;
	static bool pulse = false;
	static std::vector<Playing> playing;
	
	DateTime dt;
	getCurrentDateTime(&dt);
	
	auto Print = [&dt](char c){
		printf("\r" "%d-%02d-%02d %02d:%02d:%02d %c ", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second, c);
		fflush(stdout);
	};

	auto Pulse = [&](bool b){
		if (b) {
			wire->out_hi();
		} else {
			wire->out_lo();
		}
	};

	if (dt.second != sec) {
		// 秒が変わったら、該当フレームのパルスを開始
		sec = dt.second;
		if (dt.second == 0 || playing.size() != 60) {
			make_data(dt, &playing);
		}
		
		pulse = true;
		Pulse(pulse);
		char c = ' ';
		
		switch (playing[dt.second]) {
		case Playing::Marker: dur = 200; c = 'M'; break;
		case Playing::Value0: dur = 800; c = '0'; break;
		case Playing::Value1: dur = 500; c = '1'; break;
		}
		
		putchar('\n');		
		Print(c);
	} else if (dt.ms < dur) {
		// パルス幅が終了するまで待機
		msleep(dur - dt.ms);
	} else if (pulse) {
		// パルス OFF にして次の秒まで待機
		dur = 1000;
		pulse = false;
		Pulse(pulse);

		Print(' ');
	} else {
		std::this_thread::yield();
	}
#endif
}

int main()
{
	signal(SIGINT, sigint);

	setup();

	while (1) {
		loop();
	}

	return 0;
}

