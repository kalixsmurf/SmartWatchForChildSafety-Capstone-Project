import sys
print(sys.path)


import unittest
import json
import os
import flask
from server import app, extract_feature
from unittest.mock import patch
import numpy as np

class TestServerAPI(unittest.TestCase):
    def setUp(self):
        """Initialize fresh test environment before each test"""
        app.config['TESTING'] = True
        self.client = app.test_client()
        self.cleanup_files()

    def tearDown(self):
        """Clean up after each test"""
        self.cleanup_files()

    def cleanup_files(self):
        """Remove test files if they exist"""
        if os.path.exists("settings.txt"):
            os.remove("settings.txt")

    def test_server_initialization(self):
        """Test if Flask app exists"""
        self.assertIsNotNone(app)

    def test_api_endpoint_availability(self):
        """Test if /api/data endpoint exists"""
        response = self.client.post('/api/data')
        self.assertNotEqual(response.status_code, 404)

    def test_full_data_processing(self):
        """Test complete data processing workflow"""
        test_data = {
            "primaryPhone": "1234567890",
            "secondaryPhone": "0987654321",
            "filters": {
                "Male": 1,
                "20s": 1,
                "Happy": 1,
                "Angry": 0,  # Should be excluded
                "Disgust": 1
            }
        }

        # Verify no file exists before test
        self.assertFalse(os.path.exists("settings.txt"),
                        "File should not exist before test")

        # Send request
        response = self.client.post(
            '/api/data',
            data=json.dumps(test_data),
            content_type='application/json'
        )

        # Verify response
        self.assertEqual(response.status_code, 200)
        self.assertEqual(json.loads(response.data), {"status": "success"})

        # Verify file creation and content
        self.assertTrue(os.path.exists("settings.txt"),
                       "Settings file should be created")

        with open("settings.txt", "r") as f:
            content = f.read()
            # Required fields
            self.assertIn("1234567890", content)
            self.assertIn("0987654321", content)
            # Active filters
            self.assertIn("Male", content)
            self.assertIn("20s", content)
            self.assertIn("Happy", content)
            # Inactive filters
            self.assertNotIn("Angry", content)
            # Verify no empty lists
            self.assertNotIn("[]", content)

    def test_minimal_data_processing(self):
        """Test with only required fields"""
        response = self.client.post(
            '/api/data',
            data=json.dumps({"primaryPhone": "5551234567"}),
            content_type='application/json'
        )
        self.assertEqual(response.status_code, 200)
        
        with open("settings.txt", "r") as f:
            content = f.read()
            self.assertIn("5551234567", content)
            self.assertIn("Secondary Phone Number:", content)  # Field exists
            self.assertIn("Gender:[]", content)  # Empty list

    def test_empty_request_handling(self):
        """Test how empty JSON is handled"""
        response = self.client.post(
            '/api/data',
            data=json.dumps({}),
            content_type='application/json'
        )
        self.assertEqual(response.status_code, 200)
        
        # Verify file was created but with empty fields
        self.assertTrue(os.path.exists("settings.txt"))
        with open("settings.txt", "r") as f:
            content = f.read()
            self.assertIn("Primary Phone Number:", content)
            self.assertIn("Gender:[]", content)

    def test_malformed_request(self):
        """Test invalid JSON handling"""
        response = self.client.post(
            '/api/data',
            data="not valid json",
            content_type='application/json'
        )
        self.assertEqual(response.status_code, 400)

class TestExtractFeature(unittest.TestCase):
    @patch('server.librosa.load')
    @patch('server.librosa.feature.mfcc')
    def test_extract_feature_mfcc(self, mock_mfcc, mock_load):
        mock_load.return_value = (np.random.rand(1000), 22050)
        mock_mfcc.return_value = np.random.rand(50, 40)
        features = extract_feature("dummy.wav", mfcc=True)
        self.assertIsNotNone(features)
        self.assertEqual(features.shape[0], 40)
        #hello

    def test_extract_feature_error_handling(self):
        features = extract_feature("non_existent_file.wav", mfcc=True)
        self.assertIsNone(features)

if __name__ == '__main__':
    unittest.main()