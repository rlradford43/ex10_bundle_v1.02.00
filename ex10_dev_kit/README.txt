Welcome to the Impinj E710 Reference Design SDK!

To read your first tags:

--------------------- Hw setup ---------------------
1. Connect an antenna to Antenna port 1 (J3) and place a tag on it

2. Open a shell

--------------------- Python to C shim layer setup ---------------------
3. Move to the c SDK directory --> dev_kit/ex10_c_dev_kit

4. The make process will build the .so for the c dev kit, which gets
   used by ctypes in the py2c shim.
   - make clean
   - make
   - make install-py2c

--------------------- Running python examples ---------------------
6. Move to the python SDK directory --> dev_kit/ex10_dev_kit

7. If the impinj_reader_chip_venv is already set up, you can enter the virtual environment by typing:

	source impinj_reader_chip_venv/bin/activate

   If you do not have the py3_venv, you can create it by typing:

    source source_me_for_impinj_reader_chip_venv.sh

8. Run an example by typing:

	python3 ./examples/inventory_example.py

9. The inventory example will run for 10 seconds, reading tags and printing their EPCs to the console.

For more information, including next steps and troubleshooting, see the kit documentation on our support portal at: https://support.impinj.com/hc/en-us/articles/360011416140

For support inquiries, please email support@impinj.com with "E710 reference design" in the subject line.
