#include <stdio.h>
#include <stdlib.h>

struct
{
    unsigned char prog[446];
    struct partition
    {
        unsigned char etat;
        unsigned char chs1[3];
        unsigned char type;
        unsigned char chs2[3];
        unsigned char adrlba_premier_cluster[4];
        unsigned char taille[4];
    } tp[4];
    unsigned char aa55[2];
} mbr;

struct
{
    unsigned char x[11];
    unsigned char bytesect[2];
    unsigned char secluster;
    unsigned char zres[2];
    unsigned char x1[12];
    unsigned char adrlba_premier_cluster[4];
    unsigned char taille[4];
    unsigned char secfat[4];
    unsigned char x2[4];
    unsigned char clusteroot[2];
    unsigned char x3[455];
    unsigned char aa55[2];
} sboot;

unsigned char cluster[8193] = {'\0'};

// Fonction pour afficher toute l'arborescence a partir de la racine '/'
void afficher_arborescence(FILE *fp, unsigned long int adrdebutcluster, unsigned short int bytesecteur, unsigned char sectpcluster, unsigned long int cluster_courant, const char *nom_pere) {
    int numentree = 0;
    int adr = 0;
    int nb_entrees = (sectpcluster * bytesecteur) / 32;
    unsigned char cluster[65536] = {0};
    unsigned long int lba_premier_cluster = adrdebutcluster + (cluster_courant - 2) * sectpcluster;

    // Se positionner au cluster du répertoire
    fseek(fp, lba_premier_cluster * bytesecteur, SEEK_SET);
    fread(&cluster, sectpcluster * bytesecteur, 1, fp);

    while ((numentree < nb_entrees) && (cluster[adr] != 0x00))
    {
        if (cluster[adr] != 0xE5)
        {
            if (cluster[adr + 11] != 0x0F) // Si pas entrée de nom long
            {
                char nom[12] = {0};
                strncpy(nom, (char *)&cluster[adr], 11);
                nom[11] = '\0';

                for (int i = 10; i >= 0; i--)
                {
                    if (nom[i] == ' ')
                        nom[i] = '\0';
                    else
                        break;
                }

                unsigned char attr = cluster[adr + 11];
                char *type = (attr & 0x10) ? "Repertoire" : "Fichier";
                unsigned int taille = *(unsigned int *)&cluster[adr + 28];
                unsigned short poids_faible_cluster = *(unsigned short *)&cluster[adr + 26];
                unsigned short poids_fort_cluster = *(unsigned short *)&cluster[adr + 20];
                unsigned long cluster_entree = ((unsigned long)poids_fort_cluster << 16) | poids_faible_cluster;
                float taille_ko = taille / 1024.0;
                unsigned long lba_premier_cluster_entree = adrdebutcluster + (cluster_entree - 2) * sectpcluster;
                unsigned int taille_secteurs = (taille + bytesecteur - 1) / bytesecteur;

                // Afficher le tout sauf . et ..
                if (strncmp(nom, ".", 1) != 0 && strncmp(nom, "..", 2) != 0) {
                    printf("| %-20.11s | %17u | %12.3f | %16u | %20u | %-20s | %-22s |\n", nom, taille_secteurs, taille_ko, cluster_entree, lba_premier_cluster_entree, type, nom_pere);
                }

                // Appel Récursif pour le reste de l'arborescence
                if ((attr & 0x10) && strncmp(nom, ".", 1) != 0 && strncmp(nom, "..", 2) != 0) {
                    afficher_arborescence(fp, adrdebutcluster, bytesecteur, sectpcluster, cluster_entree, nom);
                }
            }
        }
        numentree++;
        adr += 32;
    }
}

int main(void) {
    unsigned char etat;
    unsigned char fs;
    unsigned int adr;
    unsigned long int taille;
    int nl, err;
    unsigned short int bytesecteur, sectpcluster, sectres, sizefat, clusteroot;
    unsigned long int adrdebutcluster, adrlba_premier_clusterclusteroot;

    FILE *flashdisk;

    flashdisk = fopen("/dev/sdb", "rb");
    if (flashdisk == NULL)
    {
        printf("\n Erreur : %d Le flashdisk n''est pas ouvert\n", flashdisk);
        exit(0);
    }

    nl = fread(&mbr, 512, 1, flashdisk);
    if (nl <= 0)
    {
        printf("\n erreur de lecture n = %d", nl);
        exit(0);
    }

    adr = *(long int *)&(mbr.tp[0].adrlba_premier_cluster);
    taille = *(long int *)&(mbr.tp[0].taille);
    fs = mbr.tp[0].type;

    if ((fs < 11) || (fs > 12))
    {
        printf("\nCe n'est pas une partition FAT32: Type=%2x", fs);
        exit(0);
    }

    err = fseek(flashdisk, adr * 512, SEEK_SET);
    if (err != 0)
    {
        printf("\n Erreur : %d Le deplacement n'a pas ete effectue sur le flashdisk\n", err);
        exit(0);
    }

    nl = fread(&sboot, 512, 1, flashdisk);
    if (nl <= 0)
    {
        printf("\n erreur de lecture n = %d", nl);
        exit(0);
    }

    sectpcluster = sboot.secluster;
    bytesecteur = *((short int *)&(sboot.bytesect));
    sectres = *((short int *)&(sboot.zres));
    sizefat = *((short int *)&(sboot.secfat));
    clusteroot = *((short int *)&(sboot.clusteroot));

    short int i, numfichier, nbfent, numentree, fent;
    short int nbrentree, adrntree;

    /* Adr lba_premier_cluster du premier cluster de root */
    adrdebutcluster = adr + sectres + 2 * sizefat;
    adrlba_premier_clusterclusteroot = adrdebutcluster + (clusteroot - 2) * sectpcluster;

    /* Se positionner sur le premier cluster du répertoire root */
    err = fseek(flashdisk, adrlba_premier_clusterclusteroot * bytesecteur, SEEK_SET);
    if (err != 0)
    {
        printf("\n Erreur : %d Le déplacement n'a pas été effectué sur le flashdisk\n", err);
        exit(0);
    }
    /* Lecture du secteur boot du flashdisk) */
    nl = fread(&cluster, sectpcluster * bytesecteur, 1, flashdisk);
    if (nl <= 0)
    {
        printf("\n erreur de lecture n = %d", nl);
        exit(0);
    }
    /* Nombre d’entrées d’un répertoire par cluster ; taille d’une entrée = 32 octets */
    nbrentree = (sectpcluster * bytesecteur) / 32;
    numfichier = 0;
    numentree = 0;
    adrntree = numentree * 32;
    while ((numentree < nbrentree) && (cluster[adrntree] != 0x00))
    {
        if (cluster[adrntree] != 0xE5)
        { /* si entrée valide */
            fent = numentree;
            if (cluster[adrntree + 11] == 0x0F)    /* test du format du nom */
            {                                          /* Nom format long */
                nbfent = cluster[adrntree] & 0x1F;
                adrntree = adrntree + nbfent * 32;
                numentree += nbfent;
                printf("\nFichier %0d : format long ", numfichier);
                printf("\t%02d entrees : %d -> %d", nbfent + 1, fent, fent + nbfent);
            }
            else
            { /* Nom format Court */
                printf("\nFichier %0d : format court ", numfichier);
                printf("\t01 entree : %d ", fent);
            }
        }

        numfichier++;
        adrntree += 32;
        numentree++;
    }

    /* Notre Code */

    printf("\n+----------------------+-------------------+--------------+------------------+----------------------+----------------------+------------------------+\n");
    printf("|        Nom           | Taille (secteurs) | Taille en Ko | Premier cluster  | LBA Premier Cluster  |         Type         | Nom du répertoire père |\n");
    printf("+----------------------+-------------------+--------------+------------------+----------------------+----------------------+------------------------+\n");

    afficher_arborescence(flashdisk, adrdebutcluster, bytesecteur, sectpcluster, clusteroot, "Racine");

    printf("+----------------------+-------------------+--------------+------------------+----------------------+----------------------+------------------------+\n");

    printf("\n============================= Fin =============================\n");
    fclose(flashdisk);
}