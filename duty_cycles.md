Equação -> duty cycle = (x / 125 + x) -> x = (duty cycle * 125) / (1 - duty cycle)


x = 0,9 * 125 / (1 - 0,9) = 1125 ms 

1000  32768
1125    y

y = 1125 * 32768 / 1000 = 36864


| Duty cycle | Equação | Tempo (ms) | Valor |
|------------|-------------|-------| ------|
| Duty cycle 1%| | 1ms | 33 |
| Duty cycle 10% | 14ms | 459 |
| Duty cycle 20% | 25ms | 819 |
| Duty cycle 30% | 50ms | 1638|
| Duty cycle 40% | 71ms | 2323|
| Duty cycle 50% | 63ms | 2064|
| Duty cycle 60% | 75ms | 2457|
| Duty cycle 70% | 85ms | 2793|
| Duty cycle 80% | 94ms | 3087|
| Duty cycle 90% | 113ms | 3703|

| Duty cycle | Equação | Tempo (ms) | Fator de divisão | Valor do ciclo |
|------------|-------------|-------| ------| ------|
| Duty cycle 1%| x = 0,01 * 125 / (1 - 0,01) | 1,26 ms | 794 | 33 |
| Duty cycle 10% | x = 0,1 * 125 / (1 - 0,1) | 13,89 ms | 72 | 459 |
| Duty cycle 20% | x = 0,2 * 125 / (1 - 0,2) | 31,25 ms | 32 | 819 |
| Duty cycle 30% | x = 0,3 * 125 / (1 - 0,3) | 53,57 ms | 19 |
| Duty cycle 40% | x = 0,4 * 125 / (1 - 0,4) | 83,33 ms | 12 |
| Duty cycle 50% | x = 0,5 * 125 / (1 - 0,5) | 125 ms | 8 |
| Duty cycle 60% | x = 0,6 * 125 / (1 - 0,6) | 187,5 ms | 5 |
| Duty cycle 70% | x = 0,7 * 125 / (1 - 0,7) | 292 ms  | 3 |
| Duty cycle 80% | x = 0,8 * 125 / (1 - 0,8) | 500 ms | 2 |
| Duty cycle 90% | x = 0,9 * 125 / (1 - 0,9) | 1125ms | 3703 |
