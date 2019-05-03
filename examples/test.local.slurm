#!/bin/bash
#SBATCH -p hoekstra,shared,commons
#SBATCH --time=06:00:00
#SBATCH -e test.e
#SBATCH -o test.o
#SBATCH -n 16
#SBATCH --contiguous
#SBATCH --mem=120000
#SBATCH --mail-type=ALL
#SBATCH -J test_cactus

module purge
source ~/cactus.environment
export TOIL_SLURM_ARGS="--time=0-04:00:00 --partition=hoekstra,shared,commons"
cactus --maxCores 500 --maxNodes 10000 --maxLocalJobs 160 --statePollingWait 120 --stats --binariesMode local --defaultMemory=3G --disableCaching --batchSystem slurm --logFile test.local.slurm.log /n/scratchlfs/hoekstra_lab/lassance/cactus_test/jobStore evolverMammals.txt evolverMammals.hal --root mr --writeLogs /n/scratchlfs/hoekstra_lab/lassance/cactus_test/logs/ #--logDebug #--restart
