#define F_CPU 8000000UL //8MHz takt ATmega16A mikrokontrolera
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <string.h>


#define MOTORI_PORT PORTA //pogon kotaca spojen je na PORTA
#define MOTORI_DDR DDRA
#define SENZOR_PORT PORTD //senzori su spojeni na PORTD
#define SENZOR_DDR DDRD
#define SENZOR_PIN PIND
#define PREDNJI_SENZOR 6 //"echo" prednjeg senzora spojen na PD6
#define ZADNJI_SENZOR 5 //"echo" prednjeg senzora spojen na PD5
#define DESNI_SENZOR 1 //"echo" prednjeg senzora spojen na PD1
#define LIJEVI_SENZOR 0 //"echo" prednjeg senzora spojen na PD0

volatile uint16_t udaljenost_naprijed = 0;
volatile uint16_t udaljenost_desno = 0;
volatile uint16_t udaljenost_lijevo = 0;
//volatile uint16_t udaljenost_nazad = 0;
volatile uint8_t start = 0;
volatile uint8_t smjer = 0;
volatile uint16_t brojac_kotaca = 0;
volatile uint8_t brojac_kotaca_tmp = 0;
volatile uint16_t brojac_kotaca_mem = 0;
volatile uint16_t brojac = 0; //brojac za mjerenje udaljenosti ultrazvucnim senzorom
volatile uint8_t strana = 0;
volatile uint8_t rikverc = 0;
volatile uint8_t trenutna_udaljenost = 0;
volatile uint8_t flag = 0;


void posalji_okidac(){		//šalje se okidac na jedan pin jer su okidaci svih ultrazvucnih senzora kratko spojeni. odabir pojedinog senzora bira se u funkciji dohvatiUdaljenost().
	SENZOR_PORT |=_BV(7);	//posalji stanje 1 na PIND 7
	_delay_us(10);			//pauza 10 mikrosekundi
	SENZOR_PORT &=~_BV(7);	//posalji stanje 0 na PIND 7
}

int dohvatiUdaljenost(int senzor){
	brojac = 0;	//postavi brojac u 0
	TCNT0 = 0;	//postavi TCNT0 Timera 0 u 0
	posalji_okidac();			//poziv funkcije za salnje okidaca
	
	while(bit_is_clear(SENZOR_PIN,senzor));
	TCCR0 |=_BV(CS01);	//incijalizacija Timera0, prescaler 8, 1 clk = 1 us
	TIMSK |=_BV(TOIE0);	//omoguci Timer0 overflow prekid
	
	
	while(bit_is_set(SENZOR_PIN,senzor));	//Timer0 ostaje upaljen sve dok ne dobijem 1 na "echo" pinu odabranog senzora
	TCCR0 &=~_BV(CS01);	//zaustavi timer
	TIMSK &=~_BV(TOIE0);	//onemoguci timer0 overflow prekid
	_delay_ms(40);
	return (brojac*256+TCNT0)*0.0343/2;
}
int main (){
	
	SENZOR_DDR |=_BV(7);	//trigger pin PD6
	SENZOR_DDR &= ~((1 << 6) | (1 << 5) | (1 << 1) | (1 << 0));	//echo pinovi
	SENZOR_PORT &=~_BV(7);	//postavi trigger pin PD7 u 0
	
	MOTORI_DDR = 0b11111111; //postavljanje DDRA kao izlaznog
	MOTORI_PORT = 0b11111111; //postavljanje PORTA kao izlaznog
	
	MCUCR |= _BV(ISC10) | _BV(ISC11) | _BV(ISC01) | _BV(ISC00); // rastuci brid na INT1 generira zahtjev za prekidom
	GICR |= _BV(INT1); //vanjski prekid 1 omogucen
	sei(); //omoguci globalno prekide
	
	while(1){
		
		if(start == 1){
			//
			if(rikverc){
				do{								//ako ie u rikverc gleda udaljenost zadnjeg senzora. Do ... while zbog negativnih vrijednosti senzora
					udaljenost_naprijed=dohvatiUdaljenost(ZADNJI_SENZOR);
				}while(udaljenost_naprijed < 0);
			}
			else{
				do{
					udaljenost_naprijed=dohvatiUdaljenost(PREDNJI_SENZOR);
				}while(udaljenost_naprijed < 0);
			}
			//
			if(strana == 1 || strana == 2){ //upali mjerenje puta
				GICR |= _BV(INT0);
			}
			
			while(udaljenost_naprijed > 15){ //petlja za kretati se naprijed i traziti prostor za skretanje
				udaljenost_naprijed = dohvatiUdaljenost(PREDNJI_SENZOR);
				smjer = 1;
				if(rikverc == 1){
					smjer = 2;
				}
				if(strana == 1 || strana == 3){						//ako smo iz ravnog isli lijevo
					
					do{
						udaljenost_naprijed=dohvatiUdaljenost(PREDNJI_SENZOR);
					}while(udaljenost_naprijed < 0);
					
					if(udaljenost_naprijed < 15 && strana == 3){
						brojac_kotaca_mem = 0;
						brojac_kotaca_mem = brojac_kotaca + 6;
						strana = 0;
						smjer = 0;
						break;
					}
					do{
						udaljenost_desno=dohvatiUdaljenost(DESNI_SENZOR);
					}while(udaljenost_desno < 0);
					if(udaljenost_desno > 60){
						trenutna_udaljenost = 1; //ako je dovoljno dubine omoguci brojac_kotaca_tmp u INT0_vect
						if(brojac_kotaca_tmp > 4){ //ako je brojac_kotaca_tmp veci od 18 znaci da je dovoljno dugo vremena bilo dovoljno dubine
							//broj "4" mozda malo povecat ?!?!
							if(strana == 3){	//ako je strana=3 u prekidu laserskog senzora manjuj varijablu broj_kotaca
								smjer = 4; 	//rotacija desno
								OCR2 = 185;
								_delay_ms(900);
								smjer = 0;
								strana = 6;
								brojac_kotaca = brojac_kotaca + brojac_kotaca_mem + 2;
								OCR2 = 195;
								while(brojac_kotaca > 1){	//dok broj_kotaca nije dosao u 0 idi ravno;
									smjer = 1;
									do{
										udaljenost_naprijed=dohvatiUdaljenost(PREDNJI_SENZOR);
									}while(udaljenost_naprijed < 0);
									if(udaljenost_naprijed < 15){
										strana = 8;
										start = 0;
										smjer = 0;
										break;
									}
								}
								if(strana == 8){
									break;
								}
								smjer = 3;
								OCR2 = 185;
								_delay_ms(900);
								strana = 0;
								smjer = 0;
								start = 1;
							}
							if(strana == 1){
								strana = 3;
								if(rikverc == 1){
									strana = 4;
								}
								smjer = 4; 	//rotacija desno
								OCR2 = 185;
								_delay_ms(900);
								smjer = 1;
								_delay_ms(300);
								//smjer = 0;
								rikverc = 0;
							}
							brojac_kotaca_tmp = 0;
							trenutna_udaljenost = 0;
						}
					}
					else{
						trenutna_udaljenost = 0;
					}
				}
				else if(strana == 2 ||strana == 4){ //ako smo iz ravnog isli desno
					do{
						udaljenost_naprijed=dohvatiUdaljenost(PREDNJI_SENZOR);
					}while(udaljenost_naprijed < 0);
					if(udaljenost_naprijed < 15 && strana == 4){
						brojac_kotaca_mem = brojac_kotaca + 6;
						strana = 0;
						smjer = 0;
						break;
					}
					do{
						udaljenost_lijevo=dohvatiUdaljenost(LIJEVI_SENZOR);
					}while(udaljenost_lijevo < 0);
					if(udaljenost_lijevo > 60){
						trenutna_udaljenost = 1;
						if(brojac_kotaca_tmp > 4){
							if(strana == 4){
								smjer = 3;
								OCR2 = 185;
								_delay_ms(900);
								smjer = 0;
								strana = 6;
								brojac_kotaca = brojac_kotaca + brojac_kotaca_mem + 2;
								OCR2 = 195;
								while(brojac_kotaca > 1){	//dok broj_kotaca nije dosao u 0 idi ravno;
									smjer = 1;
									do{
										udaljenost_naprijed=dohvatiUdaljenost(PREDNJI_SENZOR);
									}while(udaljenost_naprijed < 0);
									if(udaljenost_naprijed < 15){
										strana = 8;
										start = 0;
										smjer = 0;
										break;
									}
								}
								if(strana == 8){
									break;
								}
								smjer = 4;
								OCR2 = 185;
								_delay_ms(900);
								strana = 0;
								smjer = 0;
								start = 1;
							}
							if(strana == 2){
								strana = 4;
								if(rikverc == 1){
									strana = 3;
								}
								smjer = 3;
								OCR2 = 185;
								_delay_ms(900);
								smjer = 1;
								_delay_ms(d);
								//smjer = 0;

								rikverc = 0;
							}
							brojac_kotaca_tmp = 0;
							trenutna_udaljenost = 0;
						}
					}
					else{
						trenutna_udaljenost = 0;
					}
				}
				//
				if(rikverc){
					do{								//ako ie u rikverc gleda udaljenost zadnjeg senzora. Do ... while zbog negativnih vrijednosti senzora
						udaljenost_naprijed=dohvatiUdaljenost(ZADNJI_SENZOR);
					}while(udaljenost_naprijed < 0);
				}
				else{
					do{
						udaljenost_naprijed=dohvatiUdaljenost(PREDNJI_SENZOR);
					}while(udaljenost_naprijed < 0);
				}
				//
			} //kraj while
			
			if(strana == 1 || strana == 2 ){    //ako nemožemo više ravno nakon prvog skretanja treba ic u rikverc i ako je u rikvercu opet došao do kraja, stani
				
				if(rikverc == 1){
					smjer = 0;
					start=0;
					rikverc=0;
				}
				else{
					rikverc = 1;
					flag = 1;
					smjer = 2;
					//	_delay_ms(400);
				}
			}
			do{
				udaljenost_lijevo = dohvatiUdaljenost(LIJEVI_SENZOR);
			}while(udaljenost_lijevo < 0);
			
			do{
				udaljenost_desno = dohvatiUdaljenost(DESNI_SENZOR);
			}while(udaljenost_desno < 0);
			
			if(strana == 0){
				brojac_kotaca_tmp = 0;
				trenutna_udaljenost = 0;
				brojac_kotaca = 0;
				if(udaljenost_lijevo > udaljenost_desno){
					smjer = 3;
					OCR2 = 185;
					_delay_ms(900);
					strana = 1; //lijevo
					smjer = 0;
				}
				else {
					smjer = 4;
					OCR2 = 185;
					_delay_ms(900);
					strana = 2; //desno
					smjer = 0;
				}
			}
			OCR2 = 195;
			//smjer = 0;
		}
	}
}


ISR(INT0_vect){
	if(trenutna_udaljenost == 1){
		brojac_kotaca_tmp ++;
	}
	else{
		brojac_kotaca_tmp = 0;
	}
	
	if(strana == 1 || strana == 2){
		if(rikverc == 1 && flag == 1){
			if(brojac_kotaca > 0){
				brojac_kotaca--;
			}
			else{
				brojac_kotaca++;
				flag = 0;
			}
		}
		else{
			brojac_kotaca++;
		}
	}

	if(strana == 5 || strana == 6){
		brojac_kotaca--;
	}
}

ISR(INT1_vect){ //upali autic
	start = 1;
	TCCR2 |=_BV(CS21) | _BV(WGM21) | _BV(WGM20);
	OCR2 = 195;
	TIMSK |=_BV(OCIE2) | _BV(TOIE2);
}

ISR(TIMER0_OVF_vect){ //brojac za udaljenost od senzora
	brojac++;
}

ISR(TIMER2_COMP_vect){ //ugasi motore nakon 73% vremena (PWM)
	MOTORI_PORT = 0b11111111;
}

ISR(TIMER2_OVF_vect){ //promjena smjera autica
	if(smjer == 0){
		MOTORI_PORT = 0b11111111;	//stani
	}
	if(smjer == 1){
		MOTORI_PORT = 0b10011001;	//naprijed
	}
	if(smjer == 2){
		MOTORI_PORT = 0b01100110; //nazad
	}
	if(smjer == 3){
		MOTORI_PORT = 0b10101010; //lijevo
	}
	if(smjer == 4){
		MOTORI_PORT = 0b01010101; //desno
	}
}

