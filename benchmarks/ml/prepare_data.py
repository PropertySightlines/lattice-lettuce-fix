#!/usr/bin/env python3
"""
prepare_data.py - Download and convert MNIST to Salt-compatible binary format
(Zero-dependency version: No numpy required)

Output files:
  - mnist_train_images.bin  (60000 x 784 x f64)
  - mnist_train_labels.bin  (60000 x u8)
  - mnist_test_images.bin   (10000 x 784 x f64)
  - mnist_test_labels.bin   (10000 x u8)
"""

import struct
import gzip
import urllib.request
import os

MNIST_URLS = {
    'train_images': 'https://storage.googleapis.com/cvdf-datasets/mnist/train-images-idx3-ubyte.gz',
    'train_labels': 'https://storage.googleapis.com/cvdf-datasets/mnist/train-labels-idx1-ubyte.gz',
    'test_images': 'https://storage.googleapis.com/cvdf-datasets/mnist/t10k-images-idx3-ubyte.gz',
    'test_labels': 'https://storage.googleapis.com/cvdf-datasets/mnist/t10k-labels-idx1-ubyte.gz',
}

DATA_DIR = os.path.dirname(os.path.abspath(__file__))


def download_mnist():
    """Download MNIST .gz files if not present."""
    for name, url in MNIST_URLS.items():
        filename = os.path.join(DATA_DIR, f'{name}.gz')
        if not os.path.exists(filename):
            print(f'Downloading {name}...')
            urllib.request.urlretrieve(url, filename)
            print(f'  Saved to {filename}')
        else:
            print(f'{name}.gz already exists')


def process_images(gz_path, bin_path):
    """Read IDX image file, normalize to f32, write raw bytes."""
    with gzip.open(gz_path, 'rb') as f_in, open(bin_path, 'wb') as f_out:
        magic, num, rows, cols = struct.unpack('>IIII', f_in.read(16))
        assert magic == 2051, f'Invalid magic number: {magic}'
        
        total_pixels = num * rows * cols
        print(f'  Converting {num} images ({rows}x{cols})...')
        
        # Buffer read/write for speed
        chunk_size = 784 * 1000 # 1000 images at a time
        while True:
            chunk = f_in.read(chunk_size)
            if not chunk:
                break
            
            # Convert byte -> float -> byte
            floats = []
            for b in chunk:
                floats.append(float(b) / 255.0)
            
            # Pack 'f' (float32)
            f_out.write(struct.pack(f'{len(floats)}f', *floats))


def process_labels(gz_path, bin_path):
    """Read IDX label file, write raw bytes (skip header)."""
    with gzip.open(gz_path, 'rb') as f_in, open(bin_path, 'wb') as f_out:
        magic, num = struct.unpack('>II', f_in.read(8))
        assert magic == 2049, f'Invalid magic number: {magic}'
        
        print(f'  Copying {num} labels...')
        # Just copy the rest of the stream
        while True:
            chunk = f_in.read(65536)
            if not chunk:
                break
            f_out.write(chunk)


def save_for_salt():
    """Convert MNIST to Salt-compatible binary format."""
    download_mnist()
    
    # Training data
    print('\nProcessing training images...')
    process_images(os.path.join(DATA_DIR, 'train_images.gz'), 
                   os.path.join(DATA_DIR, 'mnist_train_images.bin'))
    
    print('Processing training labels...')
    process_labels(os.path.join(DATA_DIR, 'train_labels.gz'),
                   os.path.join(DATA_DIR, 'mnist_train_labels.bin'))
    
    # Test data
    print('\nProcessing test images...')
    process_images(os.path.join(DATA_DIR, 'test_images.gz'),
                   os.path.join(DATA_DIR, 'mnist_test_images.bin'))
    
    print('Processing test labels...')
    process_labels(os.path.join(DATA_DIR, 'test_labels.gz'),
                   os.path.join(DATA_DIR, 'mnist_test_labels.bin'))
    
    # Write metadata header for Salt (optional but good for ref)
    # We don't have numpy shape, but we know standard MNIST
    with open(os.path.join(DATA_DIR, 'mnist_meta.txt'), 'w') as f:
        f.write('train_count=60000\n')
        f.write('test_count=10000\n')
        f.write('image_size=784\n')
        f.write('num_classes=10\n')
    
    print('\n✓ Data preparation complete!')


if __name__ == '__main__':
    save_for_salt()
