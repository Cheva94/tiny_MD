#include "core.h"
#include "parameters.h"
#include "wtime.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>


int main()
{
    // FILE *file_xyz, *file_thermo, *file_metrica;
    // file_xyz = fopen("trajectory.xyz", "w");
    // file_thermo = fopen("thermo.log", "w");
    FILE *file_metrica;

    // NO autovectorizado: file_metrica statement clobbers memory.
    file_metrica = fopen("metrica.temp","w");
    fprintf(file_metrica,"GFLOPS\n");

    double Ekin, Epot, Temp, Pres; // variables macroscopicas
    double Rho, cell_V, cell_L, tail, Etail, Ptail;
    double *rxyz, *vxyz, *fxyz; // variables microscopicas

    // NO autovectorizado: rxyz, vxyz y fxyz statement clobbers memory.
    rxyz = (double*)malloc(3 * N * sizeof(double));
    vxyz = (double*)malloc(3 * N * sizeof(double));
    fxyz = (double*)malloc(3 * N * sizeof(double));

    printf("# Número de partículas:      %d\n", N);
    printf("# Temperatura de referencia: %.2f\n", T0);
    printf("# Pasos de equilibración:    %d\n", TEQ);
    printf("# Pasos de medición:         %d\n", TRUN - TEQ);
    printf("# (mediciones cada %d pasos)\n", TMES);
    printf("# densidad, volumen, energía potencial media, presión media\n");
    // fprintf(file_thermo, "# t Temp Pres Epot Etot\n");

    // NO autovectorizado: srand statement clobbers memory.
    srand(SEED);
    double t = 0.0, sf;
    double Rhob;
    Rho = RHOI;

    // NO autovectorizado: init_pos statement clobbers memory.
    init_pos(rxyz, Rho);

    double start = wtime();

    // Loop NO autovectorizado: contiene loops anidados.
    for (int m = 0; m < 9; m++) {
        Rhob = Rho;
        Rho = RHOI - 0.1 * (double)m;
        cell_V = (double)N / Rho;
        cell_L = cbrt(cell_V);
        tail = 16.0 * M_PI * Rho * ((2.0 / 3.0) * pow(RCUT, -9) - pow(RCUT, -3)) / 3.0;
        Etail = tail * (double)N;
        Ptail = tail * Rho;

        int i = 0;
        sf = cbrt(Rhob / Rho);

        // Loop autovectorizado usando vectores de 32 bytes
        for (int k = 0; k < 3 * N; k++) { // reescaleo posiciones a nueva densidad
            rxyz[k] *= sf;
        }

        // NO autovectorizado: init_vel y forces statement clobbers memory.
        init_vel(vxyz, &Temp, &Ekin);
        forces(rxyz, fxyz, &Epot, &Pres, &Temp, Rho, cell_V, cell_L);

        // Loop NO autovectorizado: velocity verlet statement clobbers memory.
        for (i = 1; i < TEQ; i++) { // loop de equilibracion

            velocity_verlet(rxyz, vxyz, fxyz, &Epot, &Ekin, &Pres, &Temp, Rho, cell_V, cell_L);

            sf = sqrt(T0 / Temp);

            // Loop autovectorizado usando vectores de 32 bytes
            for (int k = 0; k < 3 * N; k++) { // reescaleo de velocidades
                vxyz[k] *= sf;
            }
        }

        int mes = 0;
        double epotm = 0.0;
        double presm = 0.0;

        // Loop NO autovectorizado: control de flujo (if) y velocity verlet statement clobbers memory.
        for (i = TEQ; i < TRUN; i++) { // loop de medicion

            velocity_verlet(rxyz, vxyz, fxyz, &Epot, &Ekin, &Pres, &Temp, Rho, cell_V, cell_L);

            sf = sqrt(T0 / Temp);

            // Loop autovectorizado usando vectores de 32 bytes
            for (int k = 0; k < 3 * N; k++) { // reescaleo de velocidades
                vxyz[k] *= sf;
            }

            if (i % TMES == 0) {
                Epot += Etail;
                Pres += Ptail;

                epotm += Epot;
                presm += Pres;
                mes++;

                /*
                fprintf(file_thermo, "%f %f %f %f %f\n", t, Temp, Pres, Epot, Epot + Ekin);
                fprintf(file_xyz, "%d\n\n", N);
                for (int k = 0; k < 3 * N; k += 3) {
                fprintf(file_xyz, "Ar %e %e %e\n", rxyz[k + 0], rxyz[k + 1], rxyz[k + 2]);
                }
                */
            }

            t += DT;
        }

        printf("%f\t%f\t%f\t%f\n", Rho, cell_V, epotm / (double)mes, presm / (double)mes);

    }

    double elapsed = wtime() - start;
    printf("# Tiempo total de simulación = %f segundos\n", elapsed);
    printf("# Tiempo simulado = %f [fs]\n", t * 1.6);
    printf("# ns/day = %f\n", (1.6e-6 * t) / elapsed * 86400);
    //                       ^1.6 fs -> ns       ^sec -> day

    double gf_force = f_force/(9+TRUN);    // flop/s

    fprintf(file_metrica,"%f\n",gf_force);

    return 0;
}

