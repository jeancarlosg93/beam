#!/usr/bin/env python3
import importlib.machinery
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import subprocess
setup = importlib.machinery.SourceFileLoader("setup_output", str(Path(__file__).parents[1]/"bin/setup-output")).load_module()
class SetupTests(unittest.TestCase):
    def test_repeat_preserves_unrelated_and_backs_up(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory)/"looknfeel.lua"
            original = '-- Personal settings\no.window("other", { no_dim = false })\n'
            config.write_text(original)
            first = setup.configure(config, False)
            self.assertTrue(first["changed"])
            self.assertEqual(Path(first["backup"]).read_text(), original)
            self.assertTrue(config.read_text().startswith(original))
            self.assertEqual(config.read_text().count('^beam-output$'), 1)
            second = setup.configure(config, False)
            self.assertFalse(second["changed"])
            self.assertEqual(len(list(Path(directory).glob("*.bak.*"))), 1)
    def test_failed_validation_rolls_back(self):
        with tempfile.TemporaryDirectory() as directory:
            config=Path(directory)/"looknfeel.lua"; config.write_text('-- Personal\n')
            response=subprocess.CompletedProcess([],0,stdout="configuration error",stderr="")
            with patch.object(setup.subprocess,"run",return_value=response):
                with self.assertRaises(RuntimeError): setup.configure(config)
            self.assertEqual(config.read_text(),'-- Personal\n')
    def test_existing_exact_rule_is_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            config=Path(directory)/"looknfeel.lua"; config.write_text(setup.RULE+'\n')
            self.assertFalse(setup.configure(config,False)["changed"])
if __name__ == "__main__": unittest.main()
