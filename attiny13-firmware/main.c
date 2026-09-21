
// ATtiny13 を使った JJY 用キャリア波生成ファームウェア
// PB4 から 40kHz または 60kHz の方形波を出力し、PB2 の入力で変調（ON/OFF）を行う

#include <avr/io.h>

int main()
{
	// PB4 を出力、他のピンは入力のまま
	DDRB = 0x10;

	uint8_t c = 0;

	// PB2 の状態でキャリア周波数を選択
	// PB2 = H のとき 60kHz、L のとき 40kHz
	// 60kHz キャリア出力ループ
	while (1) {
		c ^= 0x10; // PB4 の値を反転
		PORTB = c & ((PINB << 4) & 0x10); // PB2 の入力を PB4 に反映して変調 ON/OFF
		if (PINB & 0x04) { // PB2 が H のとき 60kHz
			for (uint8_t i = 0; i < 7; i++) {
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
			}
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
		} else { // PB2 が L のとき 40kHz
			for (uint8_t i = 0; i < 11; i++) {
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
				asm volatile("nop");
			}
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
		}
	}

	return 0;
}
