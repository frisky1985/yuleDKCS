import 'package:flutter/material.dart';
import 'package:yuledkcs/yuledkcs.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const YuleDKCSExampleApp());
}

class YuleDKCSExampleApp extends StatelessWidget {
  const YuleDKCSExampleApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'YuleDKCS Demo',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.blue),
        useMaterial3: true,
      ),
      home: const HomePage(),
    );
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  bool _initialized = false;
  bool _isLoading = false;
  String _status = 'Not initialized';
  List<DigitalKey> _keys = [];
  ConnectionState _connectionState = ConnectionState.disconnected;
  
  final _apiKeyController = TextEditingController();
  final _vehicleIdController = TextEditingController();
  final _addressController = TextEditingController();

  @override
  void initState() {
    super.initState();
    YuleDKCS.connectionStateStream.listen((state) {
      setState(() {
        _connectionState = state;
      });
    });
  }

  @override
  void dispose() {
    _apiKeyController.dispose();
    _vehicleIdController.dispose();
    _addressController.dispose();
    super.dispose();
  }

  Future<void> _initialize() async {
    setState(() {
      _isLoading = true;
      _status = 'Initializing...';
    });

    try {
      await YuleDKCS.initialize(_apiKeyController.text);
      setState(() {
        _initialized = true;
        _status = 'Initialized successfully';
      });
    } catch (e) {
      setState(() {
        _status = 'Initialization failed: $e';
      });
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  Future<void> _issueKey() async {
    setState(() {
      _isLoading = true;
      _status = 'Issuing key...';
    });

    try {
      final key = await YuleDKCS.issueKey(_vehicleIdController.text);
      setState(() {
        _keys.add(key);
        _status = 'Key issued: ${key.keyId}';
      });
    } catch (e) {
      setState(() {
        _status = 'Failed to issue key: $e';
      });
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  Future<void> _listKeys() async {
    setState(() {
      _isLoading = true;
      _status = 'Listing keys...';
    });

    try {
      final keys = YuleDKCS.listKeys();
      setState(() {
        _keys = keys;
        _status = 'Found ${keys.length} keys';
      });
    } catch (e) {
      setState(() {
        _status = 'Failed to list keys: $e';
      });
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  Future<void> _connect() async {
    setState(() {
      _isLoading = true;
      _status = 'Connecting...';
    });

    try {
      await YuleDKCS.connect(_addressController.text);
      setState(() {
        _status = 'Connected';
      });
    } catch (e) {
      setState(() {
        _status = 'Connection failed: $e';
      });
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  Future<void> _sendCommand(Command command) async {
    setState(() {
      _isLoading = true;
      _status = 'Sending ${command.name}...';
    });

    try {
      await YuleDKCS.sendCommand(command);
      setState(() {
        _status = 'Command ${command.name} sent successfully';
      });
    } catch (e) {
      setState(() {
        _status = 'Failed to send command: $e';
      });
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  Future<void> _disconnect() async {
    await YuleDKCS.disconnect();
    setState(() {
      _status = 'Disconnected';
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('YuleDKCS Demo'),
        backgroundColor: Theme.of(context).colorScheme.inversePrimary,
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Status Card
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'Status: $_status',
                      style: Theme.of(context).textTheme.titleMedium,
                    ),
                    const SizedBox(height: 8),
                    Text(
                      'Connection: ${_connectionState.name}',
                      style: Theme.of(context).textTheme.bodyMedium,
                    ),
                  ],
                ),
              ),
            ),
            
            const SizedBox(height: 16),
            
            // Initialization Section
            if (!_initialized) ...[
              TextField(
                controller: _apiKeyController,
                decoration: const InputDecoration(
                  labelText: 'API Key',
                  border: OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 8),
              ElevatedButton(
                onPressed: _isLoading ? null : _initialize,
                child: const Text('Initialize SDK'),
              ),
            ],
            
            // Main Actions
            if (_initialized) ...[
              const Text(
                'Key Management',
                style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 8),
              TextField(
                controller: _vehicleIdController,
                decoration: const InputDecoration(
                  labelText: 'Vehicle ID',
                  border: OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 8),
              Row(
                children: [
                  ElevatedButton(
                    onPressed: _isLoading ? null : _issueKey,
                    child: const Text('Issue Key'),
                  ),
                  const SizedBox(width: 8),
                  ElevatedButton(
                    onPressed: _isLoading ? null : _listKeys,
                    child: const Text('List Keys'),
                  ),
                ],
              ),
              
              const SizedBox(height: 16),
              const Divider(),
              
              // BLE Connection Section
              const Text(
                'BLE Connection',
                style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 8),
              TextField(
                controller: _addressController,
                decoration: const InputDecoration(
                  labelText: 'BLE Address',
                  border: OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 8),
              Row(
                children: [
                  ElevatedButton(
                    onPressed: _isLoading ? null : _connect,
                    child: const Text('Connect'),
                  ),
                  const SizedBox(width: 8),
                  ElevatedButton(
                    onPressed: _isLoading ? null : _disconnect,
                    child: const Text('Disconnect'),
                  ),
                ],
              ),
              
              const SizedBox(height: 16),
              const Divider(),
              
              // Commands Section
              const Text(
                'Commands',
                style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 8),
              Wrap(
                spacing: 8,
                children: [
                  ElevatedButton(
                    onPressed: _connectionState == ConnectionState.ready && !_isLoading
                        ? () => _sendCommand(Command.unlock)
                        : null,
                    child: const Text('Unlock'),
                  ),
                  ElevatedButton(
                    onPressed: _connectionState == ConnectionState.ready && !_isLoading
                        ? () => _sendCommand(Command.lock)
                        : null,
                    child: const Text('Lock'),
                  ),
                  ElevatedButton(
                    onPressed: _connectionState == ConnectionState.ready && !_isLoading
                        ? () => _sendCommand(Command.startEngine)
                        : null,
                    child: const Text('Start Engine'),
                  ),
                  ElevatedButton(
                    onPressed: _connectionState == ConnectionState.ready && !_isLoading
                        ? () => _sendCommand(Command.stopEngine)
                        : null,
                    child: const Text('Stop Engine'),
                  ),
                  ElevatedButton(
                    onPressed: _connectionState == ConnectionState.ready && !_isLoading
                        ? () => _sendCommand(Command.openTrunk)
                        : null,
                    child: const Text('Open Trunk'),
                  ),
                ],
              ),
              
              const SizedBox(height: 16),
              const Divider(),
              
              // Keys List
              const Text(
                'My Keys',
                style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 8),
              Expanded(
                child: ListView.builder(
                  itemCount: _keys.length,
                  itemBuilder: (context, index) {
                    final key = _keys[index];
                    return ListTile(
                      title: Text('Key: ${key.keyId.substring(0, 8)}...'),
                      subtitle: Text('Vehicle: ${key.vehicleId}'),
                      trailing: Chip(
                        label: Text(key.status.name),
                      ),
                    );
                  },
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}